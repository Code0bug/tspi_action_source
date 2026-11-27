#!/bin/sh
# 抓当前 DRM primary plane，保存为 PNG。
# 用法: screenshot.sh [outfile]
# 当前屏幕默认是竖屏显示，截图需要顺时针旋转 90°（ffmpeg transpose=1）
set -eu
OUT=${1:-/tmp/screen.png}
ffmpeg -y -loglevel error -f kmsgrab -i - -vf hwdownload,format=bgr0,transpose=1 -frames:v 1 "$OUT"
echo "saved $OUT"
