# UI 持有 DRM master 的预览链路

实现入口：`media-service/preview_server.cpp`、`ipc/preview_protocol.c` 和
`camera-ui/platform/`。Qt 平台名为 `tspi`，使用匹配 SDK 的 Qt 5.15.2 私有
FbSupport、InputSupport API，不需要修改 Qt 源码。

## 分工与生命周期

media-service 采集单 fd NV12，使用同步 RGA 将图像顺时针旋转 90°、缩放到
UI 握手请求的物理屏幕尺寸，然后立即归还 V4L2 buffer。显示池固定为三块，
没有可写槽或 socket 发送背压时只丢预览帧，不等待 UI。未连接 UI 时不做 RGA。
原始 V4L2 fd 不跨进程。当前仓库尚未实现录像，不能将本次验证等同于录像验证。

优先通过 dma-heap 分配显示池。当前 SDK 的 RK 4.19 没有 dma-heap，因此回退到
Rockchip **render node** 的 CREATE_DUMB / PRIME 导出；它不持有 DRM master，
也不创建 framebuffer 或提交显示。枚举时检查 DRM 驱动名，避免误用 Mali 的
render node。每个 GEM handle 在导出后销毁，dma-buf fd 继续持有内存。

UI 插件的显示线程独占 `/dev/dri/card0`，设置 atomic client cap，通过
格式、possible_crtcs、zpos 范围和 TEST_ONLY 提交选取 NV12 视频层与上方的
ARGB8888 UI 层，并设置预乘 alpha 混合。初始 modeset、后续 atomic 提交、
翻页事件和退出时关闭 plane 都在此线程执行。

显示线程通过 epoll 监听 DRM、预览 socket 和 GUI 唤醒 eventfd。每次把最新
UI 和视频合入一次 NONBLOCK + PAGE_FLIP_EVENT atomic commit，最多有一个
提交等待完成。正在扫描和已提交的槽都不能归还；收到完成事件后，才归还
被替换的扫描槽。UI-only 提交继续持有同一视频槽。尚未提交的等待槽可以
被更新的一帧替换并立即归还。

GUI 使用 QFbBackingStore / QFbScreen 在普通内存中绘制和合成透明 ARGB UI。
每次合成后发布独立 QImage 快照；显示线程仅把变化区域旋转后写入空闲 dumb
buffer，不读取扫描缓冲。每个 UI buffer 分别累计 damage，避免三缓冲之间
的局部更新残留。逻辑界面尺寸为物理屏幕尺寸的转置；物理坐标为
`(physical_width - 1 - logical_y, logical_x)`，evdev 使用逆变换 `rotate=270`。
保持 Qt 的 framebuffer 直接绘制捷径关闭，不设置 QFbBackingStore 的共享扫描图像。

## Socket 协议

默认地址 `/run/tspi-camera/preview.sock`，类型 SOCK_SEQPACKET，所有数据操作
非阻塞。服务使用独立 flock 文件避免抢占已有监听 socket；权限为目录 0750、
socket 0660，接受同 uid 或 root 的客户端。默认板端以 root 启动。

所有包都是 `preview_packet` 的 72 字节本机 ABI，包含 magic、version 和
bytes。消息边界、类型、协议版本和 SCM_RIGHTS 数量均校验；错误包收到的 fd
也会被关闭。接收的描述符设置 CLOEXEC。

| 方向 | 消息 | 内容 |
| --- | --- | --- |
| UI → media | HELLO | 物理宽、高 |
| media → UI | INIT | generation、宽高、pitch、NV12、大小、固定三个 fd |
| media → UI | FRAME | generation、slot、sequence、V4L2 时间戳（微秒） |
| UI → media | RELEASE | generation、slot、对应 sequence，无 fd |

只有 INIT 携带 fd。UI 每槽执行一次 PRIME 导入和 AddFB2，两平面共享 handle，
UV offset 为 `pitch * height`；导入后关闭收到的 fd。逐帧只传元数据。
服务只接受匹配 generation、sequence 且仍由 UI 持有的槽归还。

每次连接仅初始化一个池。重新开流、改变显示尺寸时关闭通道再重新握手；
不允许在旧池仍扫描时原地替换 INIT。服务断线立即放弃旧池，重连重新分配，
不会因为 socket 已关闭就复用旧池的页。UI 断线后先等待已提交帧完成，再提交
视频 plane 关闭，收到完成事件后 RmFB / GEM_CLOSE，最后重连。UI 崩溃后新
进程重新成为 master 并导入新池。

camera-monitor 独立监管 UI 和 media-service：任一进程退出只重启该进程，
异常频繁退出时按 1–30 秒退避。收到 SIGINT / SIGTERM 时终止并回收两个子进程。
它处理进程退出；没有 GUI 心跳检测。显示线程等待 page flip 超过 3 秒或 atomic
提交失败会终止 UI，由 monitor 恢复，不能提前归还仍可能在扫描的槽。

## 编译与运行

按根 README 配置 SDK 后执行 `./tools/build.sh`。关键产物：

```text
out/rk3566/bin/media-service
out/rk3566/bin/camera-ui
out/rk3566/bin/camera-monitor
out/rk3566/plugins/platforms/libqtspi.so
out/staging/usr/lib/qt/plugins/platforms/libqtspi.so
```

构建目录中的 camera-ui 自动查找 `../plugins`，安装后自动查找
`../lib/qt/plugins`（由 CMAKE_INSTALL_LIBDIR 决定）。平台默认 tspi；可用
`QT_QPA_PLATFORM=offscreen` 在支持相应插件的环境中检查普通 UI，不能验证 DRM。
Qt 插件依赖目标 rootfs 自带的库，不把宿主机库复制到目标。

板端先停止占用 DRM master 和摄像头的旧程序，再从已部署的同一个 bin 目录启动：

```sh
unset QT_QPA_PLATFORM
/usr/bin/camera-monitor
```

也可分别启动 media-service 与 camera-ui。保留现有默认设备：预览
`/dev/video1` 请求 1920×1080，照片 `/dev/video0` 请求 3264×2448。
如果要使用与 display-camera-test 相同的预览源，设置
`TSPI_PREVIEW_CAMERA=/dev/video0`；这时不再重复打开同名照片设备。

| 环境变量 | 用途 |
| --- | --- |
| TSPI_PREVIEW_SOCKET | 双方设置相同的 socket 路径；自定义父目录需预先存在 |
| TSPI_PREVIEW_CAMERA | media-service 预览设备 |
| TSPI_DMA_HEAP | 显式选择 dma-heap；指定后失败不回退 |
| TSPI_DRM_RENDER | 无 dma-heap 时选择 Rockchip render node |
| TSPI_DRM_DEVICE | UI 使用的 DRM card，默认 /dev/dri/card0 |
| TSPI_DISABLE_INPUT=1 | 禁用 evdev 输入，供无输入设备的测试环境使用 |
| QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS | 可指定设备及 invertx / inverty；插件统一设置 rotate=270 |

控制接口仍位于 D-Bus。现有 StartCapture / StopCapture handler 仍是未接通相机
状态的骨架，本次没有把控制命令混入视频 socket，也没有补拍照或编码业务。

## 验证

主机测试不需要 Qt、RGA 或 DRM 设备：

```sh
cmake -S tests/preview -B out/preview-host -DCMAKE_BUILD_TYPE=Debug
cmake --build out/preview-host --parallel 4
(cd out/preview-host && ctest --output-on-failure)
```

覆盖 INIT fd 导入与 CLOEXEC、协议错误及截断包 fd 清理、发送背压、帧顺序、
无 SIGPIPE、正在扫描和已提交槽的生命周期、最新等待帧替换、UI-only 提交、
断线时先关闭 plane，以及真实 monitor 对模拟 UI 崩溃的独立重启和退出回收。

以下必须在 RK3566 实机验收，编译与主机测试不能替代：

1. 日志出现兼容的 video / ui plane 选择和新池导入；观察 90° 方向、满屏视频、
   透明 UI、半透明顶栏和四角触摸坐标。确认没有 EBUSY / EINVAL、撕裂或色偏。
2. 分别启动两个进程，运行 `camera-ui --test-gui-stall-ms 3000`。启动 5 秒后
   只阻塞 GUI 三秒，确认 UI 计时暂停但视频继续更新。
3. 使用 monitor 启动，记录 media-service pid，向 camera-ui 发送 SIGKILL。
   确认 UI 自动恢复、媒体 pid 不变、新连接使用新池；重复并观察 fd / GEM 占用。
4. 向 camera-ui 发送 SIGSTOP，观察 media-service 仍持续 DQBUF / QBUF，
   然后 SIGCONT 恢复；服务没有等显示槽的行为。可用 strace -f 跟踪媒体 ioctl。
5. 终止并重启 media-service，确认 UI 安全撤下旧视频层后重连；停止 monitor，
   确认无遗留子进程且可再次取得 DRM master。

目前完成本地 ARM64 编译、安装暂存和主机测试；尚无本次实现的实机验收结果。
未改动远程 BSP、Qt 或内核，也未更新 Buildroot 旧 qmake 应用包与 rootfs 镜像。
