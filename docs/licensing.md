# 许可策略

## 代码

- 主仓库代码采用 Apache-2.0。

## 数据

- 词典、频次、转换词库与模型不自动继承 Apache-2.0。
- 数据必须单独列明来源、许可和分发条件。

## 当前 alpha 策略

- 仓库内提交少量 seed 数据与生成脚本。
- `data/generated/base_lexicon.tsv` 由 `tools/lexicon-builder` 从外部开源词典生成。
- 公开 alpha 发布前，需将数据构建结果与源代码产物分开打包，并保留数据许可说明。

## 当前词典来源

- `jieba` 默认词典：MIT，用作基础词频源。
- `CC-CEDICT`：CC BY-SA 4.0，用作短语拼音和多音词校正源。
- `pypinyin`：MIT，用作剩余词条的 fallback 拼音转换。

由于生成词典现在包含 `CC-CEDICT` 派生内容，`data/generated/base_lexicon.tsv` 与其元数据不再是纯宽松许可产物，发布时必须明确保留 ShareAlike 归因要求。

## 预期后续来源

- CC-CEDICT：可作为更高覆盖短语和多音词校正来源，但需要单独归档和归因。
- OpenCC：后续简繁和地区词变体支持，Apache-2.0。
- llama.cpp：本地推理后端，按其许可证处理。

## 贡献要求

- 贡献者不得直接提交来源不明的大规模词典或模型文件。
- 新增依赖必须在 `THIRD_PARTY_NOTICES.md` 中登记。
