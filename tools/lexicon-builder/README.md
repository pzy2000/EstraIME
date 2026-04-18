# lexicon-builder

当前已提供 `Build-Lexicon.ps1`，用于把 seed 词表去重并生成 `data/generated/base_lexicon.tsv`。

```powershell
.\tools\lexicon-builder\Build-Lexicon.ps1
```

下一步：

1. 拉取外部许可允许的更大词典源。
2. 清洗成 `pinyin<TAB>text<TAB>freq` 格式。
3. 扩展成 trie/二进制词典，以支持更大规模词库。
