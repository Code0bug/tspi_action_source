# tspi-action

RK3566 相机重构工程。

## 目录与目标

| 目录 | 目标与职责 |
|------|------------|
| camera-ui/ | Qt 预览界面与 tspi atomic DRM 平台插件，UI 持有 DRM master |
| media-service/ | 摄像头采集、RGA 旋转缩放与三缓冲预览生产端 |
| hal/ | C 动态库 libcamera_hal.so.0.1.0，摄像头及硬件接口 |
| monitor/ | 独立监管 UI 与媒体服务，退出自动重启 |
| ipc/ | D-Bus 协议及 SOCK_SEQPACKET / SCM_RIGHTS 预览通道 |
| tests/ | 测试案例程序 |
| cmake/ | 可配置的交叉编译工具链 |
| tools/ | 构建、调试及辅助脚本 |
| doc/ | 重构计划与历史文档 |
| out/ | 构建、安装暂存与本机旧文件备份，Git 忽略 |

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
├── lib/qt/plugins/platforms/libqtspi.so
└── include/camera/hal.h
```

第三方运行库由目标 rootfs 提供，不重复打包。media-service 安装后的 RPATH 为 `$ORIGIN/../lib`，不包含宿主机 SDK 路径。

增量构建使用 `cmake --build out/rk3566 --parallel 4`。更换 SDK 或目标架构时使用新构建目录，不复用旧缓存。不要在主机直接运行 ARM64 程序。

预览架构、协议、运行参数与板端验收见 [doc/preview-display.md](doc/preview-display.md)。
当前已接通预览，拍照、录像及 D-Bus 控制业务尚未完成。
