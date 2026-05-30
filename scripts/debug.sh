#!/bin/bash
# RK3566 嵌入式 GDB 调试脚本（WiFi 网络直连）
# 用法: ./scripts/debug.sh [board_ip] [gdbserver_port]
#
# 前置条件：
#   板子和电脑在同一网段
#   板子上已运行: gdbserver :1234 /root/ai-cam-ui-widget

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
GDB="/usr/bin/gdb-multiarch"
BINARY="$PROJECT_DIR/build-aarch64/ai-cam-ui-widget"
BOARD_IP="${1:-10.239.132.231}"
PORT="${2:-1234}"

echo "=== RK3566 嵌入式 GDB 调试 (WiFi) ==="
echo "  GDB: $GDB"
echo "  本地二进制: $BINARY"
echo "  目标板: $BOARD_IP:$PORT"
echo ""

# 检查网络连通性
echo "[1/2] 检查网络连通性..."
if ! ping -c 1 -W 2 "$BOARD_IP" > /dev/null 2>&1; then
    echo "错误: 无法 ping 通 $BOARD_IP"
    echo "请确认板子和电脑在同一网段"
    exit 1
fi
echo "  网络正常"

# 检查二进制是否存在
if [ ! -f "$BINARY" ]; then
    echo "错误: 未找到调试二进制 $BINARY"
    echo "请先编译: cd build-aarch64 && make -j2"
    exit 1
fi

# 启动 GDB
echo "[2/2] 连接到板子 gdbserver..."
echo ""
echo "================================================"
echo "  确保板子上已运行:"
echo "    gdbserver :$PORT /root/ai-cam-ui-widget"
echo "================================================"
echo ""

exec $GDB \
    -ex "set sysroot /" \
    -ex "set solib-search-path $PROJECT_DIR/build-aarch64" \
    -ex "target remote $BOARD_IP:$PORT" \
    "$BINARY"
