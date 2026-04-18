# TSF 集成说明

## 已实现骨架

- `ITfTextInputProcessorEx`
- `ITfKeyEventSink`
- `ITfThreadMgrEventSink`
- `ITfCompositionSink`
- COM class factory
- `DllRegisterServer` / `DllUnregisterServer`

## 当前策略

- v1 alpha 先使用自绘原生候选窗，而不是 TSF UIless 候选元素。
- 这让 Notepad、浏览器 textarea、VS Code 等传统桌面场景更容易先稳定起来。
- `UIless/Search` 相关接口预留在后续里程碑。

## 关键注意点

- 不能在按键回调里进行网络或推理等待。
- 不能假设宿主支持 WinUI、COM STA 附加 UI、或任意线程模型。
- composition 写入必须通过 TSF edit session 完成。

## 注册

- 文本服务需注册 COM in-proc server。
- 文本服务需调用 `ITfInputProcessorProfiles::Register`。
- 语言 profile 需注册 `zh-CN`。

## TODO

- 增加 TSF category 注册和 UIElement 深度集成。
- 增加更准确的 caret 矩形定位。
- 增加 AppContainer 命名管道 ACL 收敛。
