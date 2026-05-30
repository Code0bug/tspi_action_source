#!/bin/sh
# 清理相机数据存储

DIRS="/userdata/imu /userdata/firstFrame /userdata/picture /userdata/video"

for dir in $DIRS; do
    if [ -d "$dir" ]; then
        count=$(ls -1 "$dir" 2>/dev/null | wc -l)
        rm -rf "$dir"/*
        echo "$dir: 已清理 $count 个文件"
    else
        echo "$dir: 目录不存在，跳过"
    fi
done
