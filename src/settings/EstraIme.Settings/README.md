# EstraIme.Settings

这是 WinUI 3 设置程序脚手架。

当前目标：

- 展示当前配置
- 切换 LLM、本地模型、隐私模式、日志级别
- 展示 sidecar 健康状态

当前限制：

- 默认 `Build-All.ps1` 不强制编译本项目
- 需要 Windows App SDK NuGet restore 后再调通完整构建

下一步：

1. 补齐 Windows App SDK 包引用版本锁定。
2. 增加配置写回和 sidecar health IPC。
3. 增加模型下载按钮与进度条。
