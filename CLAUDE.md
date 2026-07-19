# tspi_action_source

泰山派运动相机，支持实时预览、H.264/AAC MP4 录像、MP4 播放、拍照、IMU 同步录制，全程硬件编解码。

> 录像/播放数据流、零拷贝实现、音视频同步等架构细节见 [`doc/architecture.md`](doc/architecture.md)。

## 硬件平台

| 组件 | 规格 |
|------|------|
| SoC | Rockchip RK3566 |
| 摄像头 | OV8858（4-lane MIPI，自动对焦 DW9714） |
| 屏幕 | 2.3 寸 MIPI LCD（带触摸），应用内旋转 90° |
| IMU | ICM42688P（SPI + SIGIO 异步通知） |
| 按键 | 拍照/录像控制 |



## 编译

本项目使用交叉编译，工具链来自 RK Buildroot。先完成 [系统构建](#系统构建) 里的 buildroot 配置与编译。

### 工具链

先定义 SDK 根路径：

```bash
export SDK_PATH=/path/to/your/linux_sdk
```

将 `/path/to/your/linux_sdk` 替换为系统里实际 SDK 路径。工具链组件均位于 `$SDK_PATH/buildroot/output/rockchip_rk3566/` 之下：

| 组件 | 路径 |
|------|------|
| qmake（Qt 5.15.2） | `$SDK_PATH/buildroot/output/rockchip_rk3566/host/bin/qmake` |
| gcc | `$SDK_PATH/buildroot/output/rockchip_rk3566/host/bin/aarch64-buildroot-linux-gnu-gcc` |
| g++ | `$SDK_PATH/buildroot/output/rockchip_rk3566/host/bin/aarch64-buildroot-linux-gnu-g++` |
| sysroot | `$SDK_PATH/buildroot/output/rockchip_rk3566/host/aarch64-buildroot-linux-gnu/sysroot` |

> qmake / gcc / sysroot 路径由 `qmake` 生成 `build-aarch64/Makefile` 时自动写入（见 `Makefile:6` `Command:` 行）。迁 SDK 路径或改了 `.pro` 中的 sysroot 硬编码后**必须重新 qmake**，才能让 `Makefile` 里的 `CC/CXX/--sysroot` 同步刷新；`ai-cam-ui-widget.pro:56,70` 的 `INCLUDEPATH`/`LIBS` 仍是手写硬编码，需手动同步。

### 步骤

- 工程文件：`ai-cam-ui-widget.pro`（qmake，C++11）
- 输出：`build-aarch64/ai-cam-ui-widget`

1. 首次构建（生成 Makefile）

   ```bash
   mkdir -p build-aarch64
   cd build-aarch64
   $SDK_PATH/buildroot/output/rockchip_rk3566/host/bin/qmake ..
   ```

2. 增量编译（仅改了 `.cpp/.h`）

   ```bash
   cd build-aarch64
   make
   ```

3. 改了 `.pro` 或换 SDK 路径

   ```bash
   cd build-aarch64
   $SDK_PATH/buildroot/output/rockchip_rk3566/host/bin/qmake ..
   make
   ```



## 系统构建

本项目需要更新 kernel 和 buildroot（根文件系统），按下面步骤添加完之后可参考立创泰山派教程把内核和根文件系统单独烧录。所有路径基于 `$SDK_PATH`（见上节）。

### Kernel

打补丁（`resources/0001-tspi_action.patch`）：

```bash
cd $SDK_PATH/kernel
git checkout -b develop        # 第一次：创建 develop 分支
git am 0001-tspi_action.patch
```

单独编译：

```bash
cd $SDK_PATH
./build.sh kernel
```

产物：`rockdev/boot.img`。

### Buildroot

#### 1. 进入 menuconfig

```bash
cd $SDK_PATH
source buildroot/build/envsetup.sh    # 选择 65
make menuconfig
```

#### 2. Qt5

`Target packages` → `Graphic libraries and applications (graphic/text)` → `QT5`，按 Y 选中后回车进入，再勾选：

- gui module
- widgets module
- GIF support
- JPEG support
- PNG support
- fontconfig support
- qt5multimedia   # QCamera

要加 OpenGL：

- `Graphic libraries and applications` 下勾选 `OpenGL ES`
- `QT5` 下勾选 `OpenGL support`、`eglfs support`

> Qt5 已编译过需要先清构建再编 rootfs，否则可能装不进去：

```bash
cd $SDK_PATH/buildroot
make qt5base-dirclean
cd ..
./build.sh rootfs
```

编译后 Qt5 源在 `$SDK_PATH/buildroot/output/rockchip_rk3566/build/qt5base-5.15.2`。

#### 3. ffmpeg

`Target packages` → `Audio and video applications` → `ffmpeg`，全部选中。

#### 4. OpenCV

> 若要用 `cv::imread` / `cv::imwrite`，必须用 opencv4 并开启 jpeg/png 支持。

`Target packages` → `Libraries` → `Graphics` → `opencv4`，按需勾选：

```
calib3d, features2d, flann, highgui, imgcodecs, imgproc,
ml, objdetect, photo, stitching, superres, ts,
videoio, video, videostab, jpeg support, png support
```

#### 5. 保存配置

```bash
cd $SDK_PATH
make savedefconfig
```

#### 6. 应用 fs-overlay

把 `tspi_action_source/resources/fs-overlay` 整体替换到 `$SDK_PATH/buildroot/board/rockchip/rk356x/fs-overlay`，包含本项目主程序、自启动配置等定制内容。

#### 7. 编译 rootfs

```bash
cd $SDK_PATH
./build.sh rootfs
```

产物：`rockdev/rootfs.ext4`。



## 运行与部署

### 部署

将编译产物推到板子 `/root/ai-cam-ui-widget`（scp / adb / NFS 任选）。板子需已具备 `rockit`、OpenCV、FFmpeg、Qt5 运行时（均由 Buildroot rootfs 提供）。

```bash
adb push build-aarch64/ai-cam-ui-widget /root
```

### 运行

> Qt 用 `eglfs`（不再用 `linuxfb`）。4 个 Qt 环境变量（`QT_QPA_PLATFORM` / `EGLFS_INTEGRATION` / `EGLFS_ROTATION` / `GENERIC_PLUGINS`）已在板端 `/etc/profile` 预置（见 [fs-overlay](#fs-overlay)），无需每次手动 export。开机自启动由 `S60aicam` 接管。一般调试时需要把自启动关闭，在S60aicam文件中注释即可。

调试时 adb 进 shell 再启动：

```bash
adb shell
/root/ai-cam-ui-widget
```

### 数据存储

`main.cpp` 启动时自动创建 `/userdata` 下四个子目录：

| 路径 | 内容 |
|------|------|
| `/userdata/picture` | 拍照图片（PNG） |
| `/userdata/video` | 录像 MP4 |
| `/userdata/firstFrame` | 录像首帧缩略图（JPG） |
| `/userdata/imu` | 录像同步的 IMU 数据（TXT） |

清理用 `scripts/clean_storage.sh`。从板子导出：

```bash
adb pull /userdata/picture <host_path>
```



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
| `videoglwidget.cpp/.h` | 视频预览/播放的 OpenGL 渲染控件（`QOpenGLWidget` + 着色器） |
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

| 路径 | 内容 |
|------|------|
| `0001-tspi_action.patch` | 内核补丁（见 [系统构建/Kernel](#kernel)） |
| `drivers/` | 内核驱动源码：触摸（hynitron、cst226se）、背光（gp7101）、IMU（icm42688p）、摄像头（ov8858）、touch |
| `dts/` | 设备树覆盖（rockchip） |
| `fonts/` | 中英文字体（WenQuanZhengHei-1、DejaVu 全套） |
| `fs-overlay/` | Buildroot 根文件系统覆盖层（etc/root/usr），内容见下 |
| `logo/` | 开机 logo（立创、西工大、西工大反色） |
| `qt图标/` | UI 图标资源 |
| `rockit/` | rockit 媒体库（build/lib/mpi/tgi） |
| `扩展板补丁/` | 内核补丁（boot.img / kernel / kernel.patch） |
| `屏幕资料/` | ST7701S HD228001C31 屏幕规格书、初始化序列、海栎创触摸芯片移植手册 |

### fs-overlay/

Buildroot 根文件系统覆盖层（应用 [系统构建/6. 应用 fs-overlay](#6-应用-fs-overlay) 替换到板端根文件系统）：

| 路径 | 内容 |
|------|------|
| `etc/profile` | 预置 Qt eglfs/旋转/触摸环境变量（开机自动 export，见 [运行与部署](#运行与部署)） |
| `etc/init.d/S49weston` | weston 守护进程占位 init 脚本（实际 start/stop 函数体被注释） |
| `etc/init.d/S60aicam` | 启动 `ai-cam-ui-widget` 的 init 脚本（start / stop / restart） |
| `root/ai-cam-ui-widget` | 主程序二进制（与 `build-aarch64/ai-cam-ui-widget` 同步） |
| `root/*.png` | UI 图标（back / chevron-left / chevron-right / photo1 / video1） |
| `usr/lib/librockit.so` | rockit 媒体库动态库 |
| `usr/lib/fonts/` | 中英文字体（WenQuanZhengHei-1、DejaVu 全套 21 套） |

### doc/

| 文档 | 内容 |
|------|------|
| [`architecture.md`](doc/architecture.md) | 系统架构与数据流：录像/播放链路图、零拷贝实现要点、性能数据、Annex-B↔AVCC 转换、音视频同步、启动/停止流程、已知限制 |
| [`debugging.md`](doc/debugging.md) | 嵌入式调试指南：core dump 分析、strace 卡死定位、gdbserver 实时断点调试、WiFi 网络配置 |
| [`refactoring.md`](doc/refactoring.md) | 开发计划：应用层 Manager 架构重构、性能基线（2026-05-24）、内核层零拷贝优化路线、后续探索方向 |
| [`troubleshooting.md`](doc/troubleshooting.md) | 问题排查记录：录像停止 PTS 重复段错误（已修复）、录像期间 ISP EBUSY 卡死（待修复） |
| [`zerocopy-plan.md`](doc/zerocopy-plan.md) | 零拷贝链路开发计划（DMA-BUF → EGLImage → GL external texture → scanout） |

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
| `ai-cam-ui-widget.pro` | qmake 工程文件（C++11） |
| `ai-cam-ui-widget.pro.user` | Qt Creator 本地用户配置（未跟踪） |
| `include/` | 第三方头文件：`mpi/sdk`、`opencv4`、`rga`、`rkaiq` |
| `lib/` | 本地链接库（rockit 等） |
| `build-aarch64/` | 构建输出（qmake 生成，`.gitignore` 忽略） |
| `debug-artifacts/` | 调试产物（core dump 等，`.gitignore` 忽略） |
| `.vscode/` | 编辑器配置（`c_cpp_properties.json`） |
| `README.md` / `README.en.md` | 项目介绍与开发日志（硬件调试经验、各功能演进记录） |
