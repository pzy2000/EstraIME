from __future__ import annotations

import argparse
import gzip
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
CC_CEDICT_URL = "https://cc-cedict.org/editor/editor_export_cedict.php?c=gz"
CC_CEDICT_DOWNLOAD_PAGE = "https://cc-cedict.org/editor/editor.php?handler=Download"
CC_CEDICT_LICENSE_URL = "https://creativecommons.org/licenses/by-sa/4.0/"

HAN_RE = re.compile(r"^[\u3400-\u4dbf\u4e00-\u9fff]+$")
PINYIN_RE = re.compile(r"^[a-z']+$")
CC_CEDICT_LINE_RE = re.compile(r"^(?P<trad>\S+)\s+(?P<simp>\S+)\s+\[(?P<pinyin>[^\]]+)\]\s+/(?P<defs>.+)/$")


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
    parser.add_argument("--max-entries", type=int, default=140000)
    parser.add_argument("--min-freq", type=int, default=5)
    parser.add_argument("--refresh-sources", action="store_true")
    return parser.parse_args()


def ensure_download(url: str, target: Path, refresh: bool) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists() and not refresh:
        return

    with urllib.request.urlopen(url) as response, target.open("wb") as handle:
        handle.write(response.read())


def normalize_fallback_pinyin(word: str) -> str:
    syllables = lazy_pinyin(word, errors="ignore", strict=False)
    if not syllables:
        return ""

    normalized = "".join(syllables).lower()
    normalized = normalized.replace("\u00fc", "v")
    normalized = normalized.replace("u:", "v")
    return normalized


def normalize_cedict_pinyin(raw: str) -> str:
    syllables: list[str] = []
    for syllable in raw.lower().split():
        syllable = syllable.replace("\u00fc", "v").replace("u:", "v")
        syllable = re.sub(r"[1-5]", "", syllable)
        syllable = re.sub(r"[^a-z']", "", syllable)
        if not syllable:
            return ""
        syllables.append(syllable)

    return "".join(syllables)


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
        entries[(pinyin, text)] = LexiconEntry(pinyin=pinyin, text=text, freq=freq, source="seed")

    return entries


def parse_cc_cedict(cedict_gz_path: Path) -> tuple[dict[str, set[str]], dict[tuple[str, str], LexiconEntry]]:
    pinyin_overrides: dict[str, set[str]] = {}
    phrase_entries: dict[tuple[str, str], LexiconEntry] = {}

    with gzip.open(cedict_gz_path, "rt", encoding="utf-8") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue

            match = CC_CEDICT_LINE_RE.match(line)
            if not match:
                continue

            simplified = match.group("simp").strip()
            if not HAN_RE.fullmatch(simplified):
                continue

            normalized_pinyin = normalize_cedict_pinyin(match.group("pinyin"))
            if not normalized_pinyin or not PINYIN_RE.fullmatch(normalized_pinyin):
                continue

            definitions = match.group("defs").lower()
            pinyin_overrides.setdefault(simplified, set()).add(normalized_pinyin)

            # Phrase supplements are intentionally conservative.
            if len(simplified) < 2 or len(simplified) > 8:
                continue
            if definitions.startswith("surname ") or definitions.startswith("variant of ") or definitions.startswith("old variant of "):
                continue

            synthetic_freq = max(6, 18 - len(simplified) * 2)
            key = (normalized_pinyin, simplified)
            existing = phrase_entries.get(key)
            if existing is None or synthetic_freq > existing.freq:
                phrase_entries[key] = LexiconEntry(
                    pinyin=normalized_pinyin,
                    text=simplified,
                    freq=synthetic_freq,
                    source="cc-cedict-only",
                )

    return pinyin_overrides, phrase_entries


def parse_jieba_dict(
    dict_path: Path,
    min_freq: int,
    cedict_overrides: dict[str, set[str]],
) -> dict[tuple[str, str], LexiconEntry]:
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

        pinyin_candidates = cedict_overrides.get(text)
        source = "jieba"
        if pinyin_candidates:
            normalized_candidates = sorted(pinyin_candidates)
            source = "jieba+cc-cedict"
        else:
            fallback = normalize_fallback_pinyin(text)
            if not fallback or not PINYIN_RE.fullmatch(fallback):
                continue
            normalized_candidates = [fallback]

        for pinyin in normalized_candidates:
            key = (pinyin, text)
            previous = entries.get(key)
            if previous is None or freq > previous.freq:
                entries[key] = LexiconEntry(pinyin=pinyin, text=text, freq=freq, source=source)

    return entries


def merge_entries(*groups: dict[tuple[str, str], LexiconEntry]) -> list[LexiconEntry]:
    merged: dict[tuple[str, str], LexiconEntry] = {}
    for group in groups:
        for key, entry in group.items():
            existing = merged.get(key)
            if existing is None:
                merged[key] = entry
                continue

            # Prefer seed, then frequency-rich sources over synthetic phrase-only entries.
            priority = {
                "seed": 3,
                "jieba+cc-cedict": 2,
                "jieba": 1,
                "cc-cedict-only": 0,
            }
            if priority[entry.source] > priority[existing.source] or entry.freq > existing.freq:
                merged[key] = entry

    return list(merged.values())


def select_entries(entries: list[LexiconEntry], max_entries: int) -> list[LexiconEntry]:
    if max_entries <= 0 or len(entries) <= max_entries:
        return entries

    seed_entries = [entry for entry in entries if entry.source == "seed"]
    ranked_entries = [entry for entry in entries if entry.source in {"jieba", "jieba+cc-cedict"}]
    cedict_only_entries = [entry for entry in entries if entry.source == "cc-cedict-only"]

    ranked_entries.sort(key=lambda item: (-item.freq, len(item.text), item.text))
    cedict_only_entries.sort(key=lambda item: (len(item.text), -item.freq, item.text))

    remaining = max(0, max_entries - len(seed_entries))
    primary_budget = int(remaining * 0.85)
    secondary_budget = remaining - primary_budget

    selected = seed_entries + ranked_entries[:primary_budget] + cedict_only_entries[:secondary_budget]
    return selected[:max_entries]


def write_lexicon(entries: list[LexiconEntry], output_path: Path, max_entries: int) -> int:
    selected_entries = select_entries(entries, max_entries)
    selected_entries.sort(key=lambda item: (item.pinyin, -item.freq, item.text))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8", newline="\n") as handle:
        for entry in selected_entries:
            handle.write(f"{entry.pinyin}\t{entry.text}\t{entry.freq}\n")

    return len(selected_entries)


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
                "name": "cc-cedict",
                "url": CC_CEDICT_URL,
                "download_page": CC_CEDICT_DOWNLOAD_PAGE,
                "license": "CC BY-SA 4.0",
                "license_url": CC_CEDICT_LICENSE_URL,
                "role": "phrase pronunciation correction and phrase supplement source",
            },
            {
                "name": "pypinyin",
                "license": "MIT",
                "license_url": PYPINYIN_LICENSE_URL,
                "role": "fallback phrase-to-pinyin conversion",
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
    cedict_gz_path = cache_root / "cc-cedict" / "cc-cedict.gz"
    seed_path = Path(__file__).resolve().parents[2] / "data" / "seed" / "base_lexicon_seed.tsv"

    ensure_download(JIEBA_DICT_URL, jieba_dict_path, args.refresh_sources)
    ensure_download(CC_CEDICT_URL, cedict_gz_path, args.refresh_sources)

    seed_entries = parse_seed(seed_path)
    cedict_overrides, cedict_only_entries = parse_cc_cedict(cedict_gz_path)
    jieba_entries = parse_jieba_dict(jieba_dict_path, args.min_freq, cedict_overrides)
    merged_entries = merge_entries(jieba_entries, cedict_only_entries, seed_entries)

    count = write_lexicon(merged_entries, output_path, args.max_entries)
    write_metadata(output_dir, count, args.min_freq, args.max_entries)

    print(
        json.dumps(
            {
                "output": str(output_path),
                "entries": count,
                "cache_root": str(cache_root),
                "sources": ["jieba-dict", "cc-cedict", "pypinyin"],
            },
            ensure_ascii=False,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
