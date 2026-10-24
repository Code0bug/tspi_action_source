# 进程间通信

`service_ipc` 是供 UI 和媒体服务共用的 CMake INTERFACE target，提供共享头文件和 SDK 的 `dbus-1` 依赖，不生成独立库。

`include/media_service.h` 统一定义 D-Bus 服务名、对象路径、接口名，以及方法和信号名称。接口面向整个媒体服务，后续可加入音频、录像、播放等功能。当前已有相机采集控制协议，服务端实现在 `media-service/service.cpp`，UI 尚未接入。

`dbus-policy/` 存放 system bus 权限配置，构建安装时放入 `etc/dbus-1/system.d/`。

后续 `interfaces/` 可放 D-Bus XML 和帧协议，`transport/` 实现 Unix socket、SCM_RIGHTS 和序列化；帧传输当前尚未实现。
