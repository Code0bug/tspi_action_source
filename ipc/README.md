# 进程间通信

`service_ipc` 是供 UI 和媒体服务共用的 CMake INTERFACE target，提供共享头文件和 SDK 的 `dbus-1` 依赖，不生成独立库。

`include/media_service.h` 统一定义 D-Bus 服务名、对象路径、接口名，以及方法和信号名称。接口面向整个媒体服务，后续可加入音频、录像、播放等功能。当前已有相机采集控制协议，服务端实现在 `media-service/service.cpp`，UI 尚未接入。

`dbus-policy/` 存放 system bus 权限配置，构建安装时放入 `etc/dbus-1/system.d/`。

`preview_ipc` 是独立的静态库，实现非阻塞 SOCK_SEQPACKET / SCM_RIGHTS 帧通道。
`include/preview_protocol.h` 定义 HELLO / INIT / FRAME / RELEASE，只有 INIT 发送三个 fd。
`include/preview_queue.h` 管理 UI 等待、已提交、扫描中的槽，旧扫描槽只在翻页完成后归还。
协议生命周期及运行方法见 [../doc/preview-display.md](../doc/preview-display.md)。
