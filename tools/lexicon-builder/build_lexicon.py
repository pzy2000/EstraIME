from __future__ import annotations

import argparse
import json
import re
import time
import urllib.request
from dataclasses import dataclass
from pathlib import Path

from pypinyin import lazy_pinyin


JIEBA_DICT_URL = "https://raw.githubusercontent.com/fxsjy/jieba/master/jieba/dict.txt"
JIEBA_LICENSE_URL = "https://github.com/fxsjy/jieba/blob/master/LICENSE"
PYPINYIN_LICENSE_URL = "https://github.com/mozillazg/python-pinyin/blob/master/LICENSE"

HAN_RE = re.compile(r"^[\u3400-\u4dbf\u4e00-\u9fff]+$")
PINYIN_RE = re.compile(r"^[a-z']+$")


@dataclass
class LexiconEntry:
    pinyin: str
    text: str
    freq: int
    source: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build EstraIME base lexicon from open-source datasets.")
    parser.add_argument("--output", required=True, help="Directory for generated lexicon output.")
    parser.add_argument("--output-file", default="base_lexicon.tsv")
    parser.add_argument("--cache-root", required=True)
    parser.add_argument("--max-entries", type=int, default=120000)
    parser.add_argument("--min-freq", type=int, default=5)
    parser.add_argument("--refresh-sources", action="store_true")
    return parser.parse_args()


def ensure_download(url: str, target: Path, refresh: bool) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists() and not refresh:
        return

    with urllib.request.urlopen(url) as response, target.open("wb") as handle:
        handle.write(response.read())


def normalize_pinyin(word: str) -> str:
    syllables = lazy_pinyin(word, errors="ignore", strict=False)
    if not syllables:
        return ""

    normalized = "".join(syllables).lower()
    normalized = normalized.replace("\u00fc", "v")
    normalized = normalized.replace("u:", "v")
    return normalized


def parse_seed(seed_path: Path) -> dict[tuple[str, str], LexiconEntry]:
    entries: dict[tuple[str, str], LexiconEntry] = {}
    if not seed_path.exists():
        return entries

    for raw_line in seed_path.read_text(encoding="utf-8").splitlines():
        if not raw_line.strip():
            continue
        parts = raw_line.split("\t")
        if len(parts) < 3:
            continue
        pinyin, text, freq_text = parts[:3]
        try:
            freq = int(freq_text)
        except ValueError:
            continue
        key = (pinyin, text)
        entries[key] = LexiconEntry(pinyin=pinyin, text=text, freq=freq, source="seed")

    return entries


def parse_jieba_dict(dict_path: Path, min_freq: int) -> dict[tuple[str, str], LexiconEntry]:
    entries: dict[tuple[str, str], LexiconEntry] = {}
    for raw_line in dict_path.read_text(encoding="utf-8").splitlines():
        if not raw_line.strip():
            continue

        parts = raw_line.split()
        if len(parts) < 2:
            continue

        text = parts[0].strip()
        if not HAN_RE.fullmatch(text):
            continue

        try:
            freq = int(parts[1])
        except ValueError:
            continue

        if freq < min_freq:
            continue

        pinyin = normalize_pinyin(text)
        if not pinyin or not PINYIN_RE.fullmatch(pinyin):
            continue

        key = (pinyin, text)
        previous = entries.get(key)
        if previous is None or freq > previous.freq:
            entries[key] = LexiconEntry(pinyin=pinyin, text=text, freq=freq, source="jieba")

    return entries


def merge_entries(*groups: dict[tuple[str, str], LexiconEntry]) -> list[LexiconEntry]:
    merged: dict[tuple[str, str], LexiconEntry] = {}
    for group in groups:
        for key, entry in group.items():
            existing = merged.get(key)
            if existing is None or entry.freq > existing.freq:
                merged[key] = entry

    return list(merged.values())


def write_lexicon(entries: list[LexiconEntry], output_path: Path, max_entries: int) -> int:
    sorted_entries = sorted(
        entries,
        key=lambda item: (item.pinyin, -item.freq, item.text),
    )

    if max_entries > 0 and len(sorted_entries) > max_entries:
        sorted_entries = sorted(
            sorted_entries,
            key=lambda item: (-item.freq, len(item.text), item.text),
        )[:max_entries]
        sorted_entries = sorted(
            sorted_entries,
            key=lambda item: (item.pinyin, -item.freq, item.text),
        )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8", newline="\n") as handle:
        for entry in sorted_entries:
            handle.write(f"{entry.pinyin}\t{entry.text}\t{entry.freq}\n")

    return len(sorted_entries)


def write_metadata(output_dir: Path, count: int, min_freq: int, max_entries: int) -> None:
    metadata = {
        "generated_at_unix": int(time.time()),
        "entry_count": count,
        "min_freq": min_freq,
        "max_entries": max_entries,
        "sources": [
            {
                "name": "jieba-dict",
                "url": JIEBA_DICT_URL,
                "license": "MIT",
                "license_url": JIEBA_LICENSE_URL,
                "role": "base word frequency dictionary",
            },
            {
                "name": "pypinyin",
                "license": "MIT",
                "license_url": PYPINYIN_LICENSE_URL,
                "role": "phrase-to-pinyin conversion during lexicon generation",
            },
        ],
    }
    (output_dir / "base_lexicon.meta.json").write_text(
        json.dumps(metadata, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )


def main() -> int:
    args = parse_args()

    output_dir = Path(args.output)
    output_path = output_dir / args.output_file
    cache_root = Path(args.cache_root)
    cache_root.mkdir(parents=True, exist_ok=True)

    jieba_dict_path = cache_root / "jieba" / "dict.txt"
    seed_path = Path(__file__).resolve().parents[2] / "data" / "seed" / "base_lexicon_seed.tsv"

    ensure_download(JIEBA_DICT_URL, jieba_dict_path, args.refresh_sources)

    seed_entries = parse_seed(seed_path)
    jieba_entries = parse_jieba_dict(jieba_dict_path, args.min_freq)
    merged_entries = merge_entries(jieba_entries, seed_entries)

    count = write_lexicon(merged_entries, output_path, args.max_entries)
    write_metadata(output_dir, count, args.min_freq, args.max_entries)

    print(
        json.dumps(
            {
                "output": str(output_path),
                "entries": count,
                "cache_root": str(cache_root),
                "sources": ["jieba-dict", "pypinyin"],
            },
            ensure_ascii=False,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
