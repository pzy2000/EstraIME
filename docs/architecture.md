# EstraIME 架构说明

## 顶层分层

### 1. TSF Frontend

- 进程内 DLL，由 TSF 加载到宿主应用中。
- 负责按键消费、composition 生命周期、候选 UI 触发与提交。
- 绝不等待 sidecar。

### 2. Pinyin Engine

- 纯本地、同步、确定性。
- 负责拼音归一化、切分、基本排序和学习词提升。
- 是所有输入行为的安全默认路径。

### 3. Candidate Fusion

- 合并传统候选和异步 LLM 候选。
- 带 generation id、防重、置信度权重和延迟预算。

### 4. Rust Sidecar

- 运行在宿主外部，避免推理与并发逻辑污染 TSF 热路径。
- 提供 mock、llama.cpp 和未来 cloud provider 的统一接口。

### 5. Settings

- WinUI 3 桌面设置程序。
- 不嵌入宿主进程；只用于配置和健康状态展示。

## 数据流

1. 用户在文本框输入拼音。
2. TIP 先同步调用本地拼音引擎得到传统候选。
3. 若满足触发策略，TIP 通过后台线程向 sidecar 发送异步请求。
4. sidecar 返回短候选 continuation。
5. Fusion 层按规则合并并刷新候选窗。
6. 用户选词后，TIP 提交文本；学习事件异步提交给 sidecar。

## 性能边界

- 传统候选刷新预算：单次按键尽量保持在毫秒级。
- sidecar 请求必须可取消。
- composition 变化后，旧 generation 的结果全部丢弃。

## 存储边界

- 稳态配置：JSON，本地持久化。
- 静态词典：可打包资源。
- 用户学习：独立 profile 存储，支持后续快照化。

## 公开 alpha 范围

- 简体中文拼音
- 英文直通
- 候选列表、数字选词、翻页、常用标点
- 本地学习
- mock 与 llama.cpp 异步补全路径

## 暂不覆盖

- UIless/Search 深集成
- 触屏优化布局接口
- 云 provider 真正上线
