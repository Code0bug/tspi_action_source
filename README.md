# tspi-action

RK3566 相机重构工程。目前是**目录与构建骨架**，旧应用实现已移除，尚无预览、录像、回放、IPC 或监控功能。三个程序运行后直接退出，不能替代旧相机应用。

## 目录与目标

| 目录 | 目标与职责 |
|------|------------|
| camera-ui/ | Qt Widgets 最小入口，生成 camera-ui |
| media-service/ | C++ 最小入口，仅调用 HAL ABI 版本接口 |
| hal/ | C 动态库 libcamera_hal.so.0.1.0，仅实现 ABI 版本函数 |
| monitor/ | camera-monitor 最小入口，尚无 fork/监控逻辑 |
| ipc/ | camera_ipc INTERFACE target，提供 D-Bus 构建依赖，尚无协议 |
| tests/ | dependency-smoke，SDK 头文件与链接符号检查 |
| platform/ | 保留的板级驱动、精简设备树、补丁、rootfs overlay |
| cmake/ | 可配置的交叉编译工具链 |
| tools/ | 构建、调试及辅助脚本 |
| doc/ | 重构计划与历史文档 |
| out/ | 构建、安装暂存与本机旧文件备份，Git 忽略 |

内部业务目录按实现需要建立。完整规划见 [refactoring.md](doc/refactoring.md)。

## 构建依赖

主机需要 CMake ≥ 3.16、Make、pkg-config 和已构建完成的 RK3566 Buildroot SDK。所有目标依赖从 SDK sysroot 获取，不使用原根目录 include/、lib/ 副本。

- UI：Qt5 Core/Gui/Widgets、D-Bus。
- 媒体服务：HAL、D-Bus、FFmpeg（avformat/avcodec/avutil/swresample）、Threads。
- HAL：ALSA、MPP、RGA、Threads；V4L2 直接使用 Linux UAPI，无需 libv4l2 包装库。
- monitor：Threads，不依赖 Qt、HAL 或 FFmpeg。

当前 SDK 没有 Qt DBus 模块，先使用已有 dbus-1 建立依赖。Rockit、OpenCV 不属于新工程依赖。ISP/RKAIQ 接口与链接留待硬件适配阶段，本轮未验证。

## 交叉编译

在仓库根目录执行，将 SDK 路径改为本机路径：

```bash
export BUILDROOT_HOST_DIR=/path/to/linux_sdk/buildroot/output/rockchip_rk3566/host
./tools/build.sh
```

也可以分步执行：

```bash
cmake -S . -B out/rk3566 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/rk3566-buildroot.cmake \
  -DBUILDROOT_HOST_DIR="$BUILDROOT_HOST_DIR" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_INSTALL_LIBDIR=lib
cmake --build out/rk3566 --parallel 4
DESTDIR="$PWD/out/staging" cmake --install out/rk3566
```

构建产物位于 out/rk3566/bin/ 和 out/rk3566/lib/。安装暂存结构：

```text
out/staging/usr/
├── bin/camera-ui
├── bin/media-service
├── bin/camera-monitor
├── lib/libcamera_hal.so -> libcamera_hal.so.0
├── lib/libcamera_hal.so.0 -> libcamera_hal.so.0.1.0
├── lib/libcamera_hal.so.0.1.0
└── include/camera/hal.h
```

第三方运行库由目标 rootfs 提供，不重复打包。media-service 安装后的 RPATH 为 `$ORIGIN/../lib`，不包含宿主机 SDK 路径。

增量构建使用 `cmake --build out/rk3566 --parallel 4`。更换 SDK 或目标架构时使用新构建目录，不复用旧缓存。不要在主机直接运行 ARM64 程序。

## 验证范围

默认构建 dependency-smoke，真实引用 Qt Widgets、D-Bus、ALSA、MPP、RGA、FFmpeg 符号。该程序不创建窗口或打开设备，只验证头文件、链接和 ABI，不验证媒体功能。

交叉编译且没有模拟器时不注册主机 CTest 运行测试。板端可手动运行 media-service 验证 HAL 加载；dependency-smoke 不安装到 staging，需按需部署。

空入口未使用的库可能被链接器裁掉，因此第三方依赖验证以 dependency-smoke 为准，而 HAL 动态链接由 media-service 的实际函数调用验证。

## 旧文件与部署状态

旧应用源码和 qmake 工程可从 Git 历史找回。原 SDK 副本、旧构建目录和 overlay 中的旧应用/Rockit 二进制，在本工作区移到 out/legacy/ 保存，该备份不入 Git。

platform/ 内驱动、设备树、补丁和资源保留。旧自启动脚本暂时禁用应用启动，待 monitor 实现后再接入，不自动部署空骨架。

旧开发记录见 [历史 README](doc/history/legacy-readme.md)，旧系统构建资料见 [历史项目说明](doc/history/legacy-project.md)，其中旧路径和行为仅供追溯。
