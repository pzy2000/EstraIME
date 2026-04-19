# lexicon-builder

`lexicon-builder` now builds the alpha lexicon from two real upstream sources:

1. `jieba` dictionary for base words and frequencies
2. `CC-CEDICT` for phrase pronunciation overrides and phrase supplements

`pypinyin` remains the fallback for entries not covered by `CC-CEDICT`.

Outputs:

- `data/generated/base_lexicon.tsv`
- `data/generated/base_lexicon.meta.json`

Run:

```powershell
.\tools\lexicon-builder\Build-Lexicon.ps1
```

Common options:

```powershell
.\tools\lexicon-builder\Build-Lexicon.ps1 -MaxEntries 140000 -MinFreq 5 -RefreshSources
```

License notes:

- `jieba`: MIT
- `pypinyin`: MIT
- `CC-CEDICT`: CC BY-SA 4.0

That means generated lexicon artifacts now carry mixed-data obligations and must be documented accordingly before public alpha release.
