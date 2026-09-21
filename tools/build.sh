#!/bin/sh
set -eu

: "${BUILDROOT_HOST_DIR:?Set BUILDROOT_HOST_DIR to the RK3566 Buildroot host directory}"
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cmake -S "$project_dir" -B "$project_dir/out/rk3566" \
    -DCMAKE_TOOLCHAIN_FILE="$project_dir/cmake/toolchains/rk3566-buildroot.cmake" \
    -DBUILDROOT_HOST_DIR="$BUILDROOT_HOST_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_INSTALL_LIBDIR=lib
cmake --build "$project_dir/out/rk3566" --parallel "${BUILD_JOBS:-4}"
DESTDIR="$project_dir/out/staging" cmake --install "$project_dir/out/rk3566"
