# IPC 占位模块

当前 `camera_ipc` 是 CMake INTERFACE target，仅将 SDK 的 `dbus-1` 依赖提供给 UI 和媒体服务，不生成独立库。

后续 `interfaces/` 放 D-Bus XML 和帧协议，`transport/` 实现 Unix socket、SCM_RIGHTS 和序列化。当前未实现通信，不固定业务接口。
