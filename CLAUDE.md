# cam-ui-widget

Qt Widgets Rockchip 摄像头应用，支持实时预览、H.264/AAC MP4 录像、MP4 播放、拍照、IMU 同步录制，全程硬件编解码。

> 录像/播放数据流、零拷贝实现、音视频同步等架构细节见 [`doc/architecture.md`](doc/architecture.md)。

## 硬件平台

| 组件 | 规格 |
|------|------|
| SoC | Rockchip RK3566 |
| 摄像头 | OV8858（4-lane MIPI，自动对焦 DW9714） |
| 屏幕 | 2.3 寸 MIPI LCD（带触摸），应用内旋转 270° |
| IMU | ICM42688P（SPI + SIGIO 异步通知） |

## 构建

### 工具链

交叉编译，工具链来自 RK Buildroot：

- **qmake**：`/home/lzp/linux_sdk/buildroot/output/rockchip_rk3566/host/bin/qmake`（Qt 5.15.2）
- **C/C++**：`.../host/bin/aarch64-buildroot-linux-gnu-gcc/g++`
- **sysroot**：`.../host/aarch64-buildroot-linux-gnu/sysroot`
- 路径已写死在 `build-aarch64/Makefile`，工具链路径详见该文件

### 依赖库

`.pro` + Makefile 链接的库：

- Qt5：Core / Gui / Widgets
- RK media：`rockit`（VI/VENC/VDEC/AI/AO）、`rga`（2D 加速）
- OpenCV：core / highgui / imgproc / imgcodecs / video
- FFmpeg：avformat / avcodec / avutil / swresample / swscale
- 系统：pthread / atomic / mali(GPU) / drm / wayland / rt / dl

头文件位于 `include/`（mpi/sdk、opencv4、rga、rkaiq）。

### 编译

```bash
cd build-aarch64
make -j2
```

- 工程文件：`ai-cam-ui-widget.pro`（qmake，C++11）
- 输出：`build-aarch64/ai-cam-ui-widget`

### 重新生成 Makefile

修改 `.pro` 后需重新 qmake：

```bash
cd build-aarch64
/home/lzp/linux_sdk/buildroot/output/rockchip_rk3566/host/bin/qmake \
    -o Makefile ../ai-cam-ui-widget.pro \
    -spec devices/linux-buildroot-g++ CONFIG+=debug CONFIG+=qml_debug
```

### Debug 构建

带调试符号的版本（用于 core dump / gdbserver 调试，详见 `doc/debugging.md`）：

```qmake
CONFIG += c++11 debug
QMAKE_CFLAGS_DEBUG += -g
QMAKE_CXXFLAGS_DEBUG += -g
```

验证：`readelf -S build-aarch64/ai-cam-ui-widget | grep debug`

### 已知 warning

`videodecoder.cpp` 有一个关于 `FRAME_FLAG_SNAP_END` 的 signed/unsigned warning，是既有问题，忽略即可。

## 运行与部署

### 部署到板子

编译产物部署到板子 `/root/ai-cam-ui-widget`（通过 scp / adb / NFS，视环境而定）。板子需已具备 `rockit`、OpenCV、FFmpeg、Qt5 运行时（均由 Buildroot rootfs 提供）。

### 数据存储

`main.cpp` 启动时自动创建 `/userdata` 下四个子目录：

| 路径 | 内容 |
|------|------|
| `/userdata/picture` | 拍照图片（PNG） |
| `/userdata/video` | 录像 MP4 |
| `/userdata/firstFrame` | 录像首帧缩略图（JPG） |
| `/userdata/imu` | 录像同步的 IMU 数据（TXT） |

清理用 `scripts/clean_storage.sh`。

### 屏幕与字体

- `main.cpp` 用 `QGraphicsView` + `setRotation(270)` 旋转 UI 适配竖屏
- 字体：板子 `/usr/lib/fonts/WenQuanZhengHei-1.ttf`，启动时 `QFontDatabase::addApplicationFont` 加载
- `SIGINT` → `QApplication::quit()`（便于 Ctrl+C 退出）

### 开机自启动

通过 `resources/fs-overlay/`（Buildroot 文件系统覆盖层）配置，详见该目录与 README 开发记录。

### WiFi 联网

板子仅支持 2.4GHz WiFi，用 `scripts/connect_wifi.sh` 一键连接（调试/GDB 远程必需）。

## 文件结构

### 源代码

| 文件 | 职责 |
|------|------|
| `main.cpp` | 程序入口：创建 `/userdata` 目录、屏幕旋转 270°、加载字体、SIGINT 退出 |
| `widget.cpp/.h` | UI 初始化、worker 线程创建、信号槽连接、相册/视频列表 |
| `widget.ui` | Qt Designer UI |
| `cameramanager.cpp/.h` | VI 摄像头采集、预览帧 RGA 转换、录像零拷贝路径、首帧缩略图 |
| `videoencoder.cpp/.h` | BGR 帧送 RK VENC 编码 H.264，发给 MP4 封装器 |
| `mp4muxer.cpp/.h` | MP4 封装：Annex-B↔AVCC 转换、AAC 编码、音视频交织写入 |
| `mp4demuxer.cpp/.h` | MP4 解封装：视频送 VDEC，音频 packet 发给 AudioManager |
| `videodecoder.cpp/.h` | 轮询 VDEC 解码帧，RGA 转 BGR，发给 UI 显示 |
| `audiomanager.cpp/.h` | RK AI 采集 + AO 播放 + FFmpeg 音频编解码（AAC↔PCM） |
| `imureader.cpp/.h` | ICM42688P SPI 采集（SIGIO 异步通知），录像时同步记录 IMU 数据 |
| `keycontrol.cpp/.h` | 轮询 input 事件按键，发拍照/录像/切界面信号 |
| `savepicture.cpp/.h` | 独立线程保存照片，避免拍照时 UI 卡顿 |

### utils/

| 文件 | 职责 |
|------|------|
| `debuglog.h` | 调试日志宏，追加写 `/tmp/debug.log`，每次 `fflush`（崩溃前最后一行定位崩溃点） |
| `rk_mpi.cpp/.h` | RK MPI 初始化封装（`vdec_init`、`venc_init`） |
| `safeQueue.h` | 线程安全队列模板 `SafeQueue<T>` |

### resources/

板子 BSP 与定制资源（非应用代码）：

| 子目录 | 内容 |
|--------|------|
| `drivers/` | 内核驱动源码：触摸（hynitron、cst226se）、背光（gp7101）、IMU（icm42688p）、摄像头（ov8858）、touch |
| `dts/` | 设备树覆盖（rockchip） |
| `fonts/` | 字体文件 |
| `fs-overlay/` | Buildroot 文件系统覆盖层（etc/root/usr），含开机自启动配置 |
| `logo/` | 开机 logo（立创、西工大） |
| `qt图标/` | UI 图标资源 |
| `rockit/` | rockit 媒体库（build/lib/mpi/tgi） |
| `扩展板补丁/` | 内核补丁 |
| `海栎创触摸芯片移植手册-v3.5-20220701.pdf` | 触摸芯片移植参考 |

### doc/

| 文档 | 内容 |
|------|------|
| [`architecture.md`](doc/architecture.md) | 系统架构与数据流：录像/播放链路图、零拷贝实现要点、性能数据、Annex-B↔AVCC 转换、音视频同步、启动/停止流程、已知限制 |
| [`debugging.md`](doc/debugging.md) | 嵌入式调试指南：core dump 分析、strace 卡死定位、gdbserver 实时断点调试、WiFi 网络配置 |
| [`refactoring.md`](doc/refactoring.md) | 开发计划：应用层 Manager 架构重构、性能基线（2026-05-24）、内核层零拷贝优化路线、后续探索方向 |
| [`troubleshooting.md`](doc/troubleshooting.md) | 问题排查记录：录像停止 PTS 重复段错误（已修复）、录像期间 ISP EBUSY 卡死（待修复） |

### scripts/

| 脚本 | 内容 |
|------|------|
| `clean_storage.sh` | 清理 `/userdata` 下 imu/firstFrame/picture/video 目录 |
| `connect_wifi.sh` | 一键连接 WiFi（用法 `./connect_wifi.sh [ssid] [password]`，默认 SSID=123） |
| `debug.sh` | 宿主机 GDB 远程调试，连接板子 `gdbserver :1234`（用法 `./scripts/debug.sh [board_ip] [port]`） |
| `gdbserver` | aarch64 gdbserver 二进制（从 Buildroot debug-root 提取，部署到板子） |

### 其他目录

| 路径 | 说明 |
|------|------|
| `include/` | 第三方头文件：`mpi/sdk`、`opencv4`、`rga`、`rkaiq` |
| `lib/` | 本地链接库（rockit 等） |
| `build-aarch64/` | 构建输出（qmake 生成，`.gitignore` 忽略） |
| `debug-artifacts/` | 调试产物（core dump 等，`.gitignore` 忽略） |
| `.vscode/` | 编辑器配置（`c_cpp_properties.json`） |
| `README.md` / `README.en.md` | 项目介绍与开发日志（硬件调试经验、各功能演进记录） |
