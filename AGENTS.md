# tspi-action-source

泰山派 RK3566 运动相机的应用层。CMake 交叉编译到 aarch64，产物只在开发板上跑，不要在宿主机执行。BSP / 内核 / Buildroot 在远程 SDK，本仓库只改应用。

双进程：`camera-ui` 画界面，`media-service` 做采集。预览走 Unix socket 传 overlay dma-buf fd（SCM_RIGHTS）；采集启停走 system D-Bus（`com.tspi.MediaService`）。UI 自定义 QPA 插件 `qtspi`：primary plane 画 Qt，overlay plane 画相机预览。当前 UI 是占位界面，尚未接 D-Bus。

## 角色

用户负责采集、HAL、IPC 和 BSP 联调。**UI 由 Agent 完成**：实现 `camera-ui/`（含 `platform/`）的界面，并按「UI 调试」闭环编译、上板、截屏验收，直到板端画面符合要求。

## 目录


| 目录               | 职责                                                                          |
| ---------------- | --------------------------------------------------------------------------- |
| `camera-ui/`     | Qt Widgets 应用；`platform/` 是 QPA 插件 `libqtspi.so`，启动需 `QT_QPA_PLATFORM=tspi` |
| `media-service/` | 采集与预览服务：V4L2 + RGA 写入 overlay，并导出 D-Bus 控制                                  |
| `hal/`           | `libcamera_hal.so.0.1.0`，C ABI：camera（V4L2）、display（DRM）、IMU、按键             |
| `ipc/`           | 预览 Unix socket、D-Bus 协议头和 system bus 策略；`service_ipc` 是 INTERFACE target    |
| `monitor/`       | `camera-monitor`，尚未实现                                                       |
| `tests/`         | 板端 HAL / 显示 / IPC 测试程序                                                      |
| `tools/`         | 构建、adb 调试、截屏等脚本                                                             |
| `cmake/`         | RK3566 Buildroot 交叉编译工具链                                                    |
| `doc/`           | 调试与排障笔记                                                                     |
| `out/`           | 构建与 staging 产物，Git 忽略                                                       |


# 构建和运行

## 构建

在仓库根目录：

```bash
export BUILDROOT_HOST_DIR=/home/lzp/linux_sdk/buildroot/output/rockchip_rk3566/host
./tools/build.sh
```

产物在 `out/staging/usr/`（`bin/`、`lib/`、`lib/qt/plugins/platforms/`）。把它们同步到板上对应路径后再运行。不要在宿主机直接跑这些二进制。

## 运行

先拷到板端：

```bash
adb shell mkdir -p /usr/lib/qt/plugins/platforms /etc/dbus-1/system.d
adb push out/staging/usr/bin/media-service /usr/bin/
adb push out/staging/usr/bin/camera-ui /usr/bin/
adb push out/staging/usr/lib/libcamera_hal.so.0.1.0 /usr/lib/
adb push out/staging/usr/lib/qt/plugins/platforms/libqtspi.so /usr/lib/qt/plugins/platforms/
adb push out/staging/usr/etc/dbus-1/system.d/com.tspi.MediaService.conf /etc/dbus-1/system.d/
adb shell 'ln -sf libcamera_hal.so.0.1.0 /usr/lib/libcamera_hal.so.0; ln -sf libcamera_hal.so.0 /usr/lib/libcamera_hal.so'
```

板上先起媒体服务，再起 UI：

```bash
media-service
QT_QPA_PLATFORM=tspi camera-ui
```

## UI 调试

改界面必须走下面闭环，不以宿主机渲染当验收。**Agent 只能改 `camera-ui/`（含 `platform/`）**。`media-service/`、`hal/`、`ipc/`、`tools/`、`cmake/`、`monitor/`、`tests/` 等其它目录未经用户允许不得修改、不得顺手重构。

`screenshot.sh` 只抓 DRM **primary plane**（Qt UI）。相机预览在 overlay，截图里看不到视频层；不要为了「图里没有预览」去改 HAL 或 media-service。

### 闭环

1. 在 `camera-ui/` 改界面。
2. 交叉编译（不要在宿主机跑产物）：

```bash
export BUILDROOT_HOST_DIR=/home/lzp/linux_sdk/buildroot/output/rockchip_rk3566/host
./tools/build.sh
```

3. 同步到板。只改应用代码时推 `camera-ui`；改了 `platform/` 再推 `libqtspi.so`：

```bash
adb push out/staging/usr/bin/camera-ui /usr/bin/
adb push out/staging/usr/lib/qt/plugins/platforms/libqtspi.so /usr/lib/qt/plugins/platforms/
```

4. 板上重启 UI。另开 adb 命令，不要占用已有交互 `adb shell`：

```bash
adb shell 'killall camera-ui 2>/dev/null; QT_QPA_PLATFORM=tspi nohup camera-ui >/tmp/camera-ui.log 2>&1 &'
```

等界面画完再截屏。不要重启或改动 `media-service`，除非用户明确要求。

5. 板上截屏并拉回本机。本地文件带时间戳，不要覆盖旧图：

```bash
SHOT="out/screenshots/screen-$(date +%Y%m%d-%H%M%S).png"
mkdir -p out/screenshots
adb push tools/screenshot.sh /tmp/screenshot.sh
adb shell 'sh /tmp/screenshot.sh /tmp/screen.png'
adb pull /tmp/screen.png "$SHOT"
```

6. 读取刚拉回的 `$SHOT`（`out/screenshots/screen-YYYYMMDD-HHMMSS.png`），对照本次改动的控件、文案、位置、颜色验收。不对就只改 `camera-ui/`，从第 2 步再走一遍。

