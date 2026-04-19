# EstraIME

EstraIME 是一个面向 Windows 11 的开源中文输入法 alpha，目标是提供一个真实可安装的 TSF IME，并在不阻塞主输入路径的前提下，把异步 LLM 自动补全融合进传统候选列表。

## 当前状态

- 仓库已包含可演进的 alpha 代码骨架。
- C++ 主体覆盖 TSF TIP、拼音引擎、候选窗、配置读取和 sidecar IPC 客户端。
- Rust sidecar 覆盖 mock provider、llama.cpp OpenAI-compatible adapter、配置存储和命名管道服务。
- WinUI 3 设置程序与 WiX 安装器已建立工程级脚手架。
- v1 仍有明确限制，见 [docs/known-limitations.md](docs/known-limitations.md)。

## 仓库结构

```text
build/                  PowerShell 构建、运行、打包脚本
data/                   词典 seed、schema、生成产物
docs/                   架构、集成、许可和 alpha 计划
installer/wix/          WiX 打包脚手架
shared/                 C++ 共享头和 JSON schema
sidecar/                Rust workspace
src/common/             C++ 公共工具
src/engine/             拼音引擎
src/fusion/             候选融合
src/ui/                 原生 HWND 候选窗
src/tip/                TSF TIP DLL
src/settings/           WinUI 3 设置程序脚手架
tests/                  C++/Rust/集成夹具
```

## 构建前置条件

### 必需

- Windows 11
- Visual Studio 2022 Build Tools，包含：
  - MSVC v143
  - Windows 10/11 SDK
  - C++ ATL/MFC 支持不是必需
- Rust stable toolchain

### 可选

- WiX Toolset 3.x/4.x，用于生成安装包
- Windows App SDK NuGet restore 能力，用于编译 WinUI 3 设置程序
- llama.cpp server，可选本地推理后端

## 快速开始

```powershell
pwsh -File .\build\Bootstrap-DevEnv.ps1
pwsh -File .\build\Restore-Dependencies.ps1
pwsh -File .\build\Build-All.ps1
pwsh -File .\tools\lexicon-builder\Build-Lexicon.ps1
```

默认构建：

- C++ TIP DLL 与引擎测试
- Rust sidecar workspace
- `lexicon-builder` 可生成大规模基础词典
- 不默认编译 WinUI 3 设置程序，因为当前环境通常缺少 Windows App SDK NuGet cache

如需尝试设置程序：

```powershell
pwsh -File .\build\Build-All.ps1 -IncludeSettings
```

## 开发运行

1. 启动 sidecar：

```powershell
.\build\Start-Sidecar.ps1
```

2. 构建并注册 TIP：

```powershell
pwsh -File .\build\Run-DevIME.ps1 -StartSidecar
```

3. 在 Windows 输入法设置里启用 `EstraIME Alpha`。
4. 使用 `.\build\Test-Sidecar.ps1` 检查 sidecar 健康状态。
5. 使用 `.\build\Test-TypingCheckList.ps1` 跑手工联调 checklist。

## 词典构建

当前词典生成链路已经升级为双源开源数据管线：

- `jieba` 默认词典提供基础词频
- `CC-CEDICT` 提供短语拼音和多音词校正
- `pypinyin` 提供剩余词条的 fallback 拼音转换
- 仓库内 seed 词典继续用于强制补齐 alpha 关键词

生成命令：

```powershell
.\tools\lexicon-builder\Build-Lexicon.ps1 -MaxEntries 140000 -MinFreq 5
```

产物：

- `data/generated/base_lexicon.tsv`
- `data/generated/base_lexicon.meta.json`

## 设计原则

- 主输入路径绝不等待 LLM。
- LLM 永远是增益层，不是基础转换层。
- sidecar 失效时，传统拼音输入仍完整可用。
- 默认本地优先，云增强关闭。
- 配置与日志默认隐私保守。

## 已知非目标

- 双拼、五笔、多语言平台
- OCR、翻译、语音、云同步、皮肤系统、插件生态

## 开源与第三方

- 代码许可证：Apache-2.0
- 第三方与数据许可策略见 [docs/licensing.md](docs/licensing.md) 和 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)

## 安装与发布说明

当前仓库更接近“开发者 alpha 骨架 + 最小纵切”：

- 传统拼音路径与 sidecar 异步路径均已落地代码
- 安装器与设置程序已给出真实工程脚手架
- 已支持页内选中与真正分页
- 已支持开发态 sidecar 自动拉起与词典生成脚本
- 搜索/UIless/触屏键盘深集成仍列为后续增强

如要对外发布 public alpha，先完成：

- 签名
- WiX 打包验证
- AppContainer 与 Edge/VS Code 回归
- 模型下载与 llama.cpp 打包策略确认
