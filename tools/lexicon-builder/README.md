# lexicon-builder

`lexicon-builder` 现在会从真实开源词典生成 `data/generated/base_lexicon.tsv`。

当前管线：

1. 从 `jieba` 默认词典下载高频词和频次。
2. 使用 `pypinyin` 为词条批量生成无声调拼音。
3. 合并仓库内的人工 seed 词条。
4. 生成：
   - `data/generated/base_lexicon.tsv`
   - `data/generated/base_lexicon.meta.json`

运行方式：

```powershell
.\tools\lexicon-builder\Build-Lexicon.ps1
```

常用参数：

```powershell
.\tools\lexicon-builder\Build-Lexicon.ps1 -MaxEntries 150000 -MinFreq 3 -RefreshSources
```

当前来源：

- `jieba` 词典：MIT
- `pypinyin`：MIT

后续增强：

1. 引入额外短语词典和更高质量的多音词覆盖。
2. 生成二进制 trie，降低 TIP 冷启动开销。
3. 加入领域词典分层和用户词典快照合并。
