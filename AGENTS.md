# TSPI-ACTION-SOURCE

## 介绍

本仓库是泰山派 RK3566 运动相机的应用层，与 `tspi_action_bsp` 配套使用。BSP、内核和 Buildroot 系统配置由配套 SDK 维护。日常开发使用 Buildroot 工具链和 CMake 交叉编译为 aarch64，通过 adb 部署到泰山派运行和验收。

应用由两个进程组成。`camera-ui` 使用 Qt Widgets，负责界面、触摸交互和相册浏览。`media-service` 负责音视频采集、拍照、录像和视频回放。UI 通过 system D-Bus 服务 `com.tspi.MediaService` 控制采集启停和视频回放，并接收拍照、录像状态通知。

`camera-ui` 的 `qtspi` 平台插件将 Qt 界面绘制在 DRM primary plane，将相机预览和视频回放显示在 overlay plane。UI 通过 Unix socket 将 overlay 缓冲区的 dma-buf fd 交给媒体服务，媒体服务写入画面后通过同一个 socket 通知 UI 更新显示。

## 目录

| 目录 | 职责 |
| --- | --- |
| `camera-ui/` | Qt Widgets 界面、相册和 D-Bus 客户端。`platform/` 构建 QPA 插件 `libqtspi.so`，用 `QT_QPA_PLATFORM=tspi` 启动。 |
| `media-service/` | 音视频采集、预览、拍照、录像和视频回放，处理实体按键及录像期间的 IMU 数据记录，提供 D-Bus 服务。 |
| `hal/` | 摄像头（V4L2）和音频（ALSA）的 C 接口，构建 `libmedia_hal.so`。目录内的 DRM 显示实现由板端测试直接编译。 |
| `ipc/` | `ipc_socket` 实现 Unix socket 通信和 fd 传递。`service_ipc` 提供共享 D-Bus 协议头及通信依赖，`dbus-policy/` 存放 system bus 策略。 |
| `tests/` | 板端摄像头与显示联调、显示色彩和 Unix socket 通信测试。 |
| `tools/` | 交叉编译、调试、板端截屏、连接 Wi-Fi 和清理存储的脚本及工具。 |
| `cmake/` | RK3566 Buildroot 交叉编译工具链配置。 |
| `doc/` | 项目相关记录文档。 |
| `out/` | 本地生成目录，包含 `rk3566/` 构建产物、`staging/` 安装产物和 `screenshots/` 板端截图，由 Git 忽略。 |

## 项目框架

> 待补充：双进程框架、显示和播放链路、各cpp类角色定位

## 编译和调试

### 编译

先在用户目录下检查是否已有 `tspi_action_bsp` 和已编译的 Linux SDK（内核版本为 4.19.232）。如果缺少配套仓库或 SDK，提醒用户按 [README](README.md#快速开始) 下载并配置。

在仓库根目录，将 `BUILDROOT_HOST_DIR` 设为本机 SDK 的 Buildroot `host` 目录，再运行编译脚本。

```bash
export BUILDROOT_HOST_DIR=/path/to/linux_sdk/buildroot/output/rockchip_rk3566/host
./tools/build.sh
```

产物位于 `out/staging/usr/` 下的 `bin/`、`lib/` 和 `lib/qt/plugins/platforms/`，部署到板端后运行。

### 板端调试

在开发环境安装 adb，安装方法见 [README](README.md#快速开始)。

```bash
adb shell                   # 进入板端终端
adb push 本地文件路径 板端路径  # 上传文件
adb pull 板端文件路径 本地路径  # 下载文件
```

按改动范围同步对应文件。

```bash
# 若更新了 media-service
adb push out/staging/usr/bin/media-service /usr/bin/
# 若更新了 camera-ui
adb push out/staging/usr/bin/camera-ui /usr/bin/
# 若更新了 HAL
adb push out/staging/usr/lib/libmedia_hal.so /usr/lib/
# 若更新了 camera-ui/platform
adb push out/staging/usr/lib/qt/plugins/platforms/libqtspi.so /usr/lib/qt/plugins/platforms/
# 若更新了 D-Bus 策略
adb push out/staging/usr/etc/dbus-1/system.d/com.tspi.MediaService.conf /etc/dbus-1/system.d/
```

同步后检查 `media-service` 和 `camera-ui` 的可执行权限，必要时在板端执行 `chmod`。

```bash
chmod +x /usr/bin/media-service /usr/bin/camera-ui
```

板端的 `S23tspiAction` 已适配这两个进程时，可用相机启动脚本启停应用。

```bash
/etc/init.d/S23tspiAction start
/etc/init.d/S23tspiAction stop
```

### UI 调试

进行 UI 调试时，**Agent 只能改 `camera-ui/`（含 `platform/`）**。`media-service/`、`hal/`、`ipc/`、`tools/`、`cmake/`、`monitor/`、`tests/` 等其它目录未经用户允许不得修改。

`screenshot.sh` 只抓 DRM **primary plane**（Qt UI）。相机预览在 overlay，截图里看不到视频层；不要为了「图里没有预览」去改 HAL 或 media-service。

1. 在 `camera-ui/` 改界面。

2. 按[编译](#编译)一节交叉编译。

3. 同步产物之前，按[板端调试](#板端调试)一节，先停止相机应用，然后同步产物，同步后再启动。只改UI应用代码时同步 `camera-ui`，改了 `platform/` 再同步 `libqtspi.so`。

4. 用户没有要求自行截图调试时，启动相机应用后就停止。

5. 用户要求截图调试时，在本机仓库根目录执行以下命令。

   ```bash
   SHOT="out/screenshots/screen-$(date +%Y%m%d-%H%M%S).png"
   mkdir -p out/screenshots
   adb push ./tools/screenshot.sh /tmp/screenshot.sh
   adb shell 'sh /tmp/screenshot.sh /tmp/screen.png' && adb pull /tmp/screen.png "$SHOT"
   ```

   打开刚拉回的 `$SHOT`，对照用户要求检查控件、文案、位置和颜色。需要调整时，修改后重新编译、部署并截图复查。
