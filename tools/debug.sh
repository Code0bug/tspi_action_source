#!/bin/bash
# Usage: tools/debug.sh <board_ip> [camera-ui|media-service|camera-monitor] [port]
# On board: gdbserver :1234 /usr/bin/media-service
set -eu
: "${BUILDROOT_HOST_DIR:?Set BUILDROOT_HOST_DIR to the RK3566 Buildroot host directory}"
board_ip="${1:?Usage: tools/debug.sh board_ip [target] [port]}"
target="${2:-media-service}"
port="${3:-1234}"
case "$target" in
    camera-ui|media-service|camera-monitor) ;;
    *) echo "Unknown target: $target" >&2; exit 1 ;;
esac
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
binary="$project_dir/out/rk3566/bin/$target"
test -f "$binary" || { echo "Build first: tools/build.sh" >&2; exit 1; }
exec gdb-multiarch \
    -ex "set sysroot $BUILDROOT_HOST_DIR/aarch64-buildroot-linux-gnu/sysroot" \
    -ex "set solib-search-path $project_dir/out/rk3566/lib" \
    -ex "target remote $board_ip:$port" "$binary"
