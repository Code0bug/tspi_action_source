# tspi-action

RK3566 相机重构工程。

## 目录与目标

| 目录 | 目标与职责 |
|------|------------|
| camera-ui/ | ui 应用目录 |
| media-service/ | media 应用服务 |
| hal/ | C 动态库 libcamera_hal.so.0.1.0，仅实现 ABI 版本函数 |
| monitor/ | camera-monitor 最小入口 |
| ipc/ | service_ipc INTERFACE target，提供共享 D-Bus 协议、构建依赖和总线权限配置 |
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
└── include/camera/hal.h
```

第三方运行库由目标 rootfs 提供，不重复打包。media-service 安装后的 RPATH 为 `$ORIGIN/../lib`，不包含宿主机 SDK 路径。

增量构建使用 `cmake --build out/rk3566 --parallel 4`。更换 SDK 或目标架构时使用新构建目录，不复用旧缓存。不要在主机直接运行 ARM64 程序。