# Example toolchain.cmake content
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)


set(CMAKE_C_COMPILER "/home/lzp/linux_sdk/buildroot/output/rockchip_rk3566/host/bin/aarch64-buildroot-linux-gnu-gcc")
set(CMAKE_CXX_COMPILER "/home/lzp/linux_sdk/buildroot/output/rockchip_rk3566/host/bin/aarch64-buildroot-linux-gnu-g++")

# 设置目标系统根目录
set(CMAKE_FIND_ROOT_PATH /home/lzp/linux_sdk/buildroot/output/rockchip_rk3568/target/)

# 配置 CMake 查找程序和库文件的方式
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

