#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import subprocess

IMU_DIR = "imu"
VIDEO_DIR = "video"

tscale_val = 0.02  # 20ms
gscale_val = 0.01745329252 * 1000.0 / 32768.0  # π / 180
ascale_val = 0.001 * 4000 / 32768.0

one_file_name = "20170804_090227"

def txt_to_gyroflow_csv(name,
                        tscale=0.001,
                        gscale=0.00122173047,
                        ascale=0.00048828125):
    """
    imu/<name>.txt -> imu/<name>.csv
    """
    txt_file = os.path.join(IMU_DIR, name + ".txt")
    csv_file = os.path.join(IMU_DIR, name + ".csv")

    if not os.path.exists(txt_file):
        print(f"❌ IMU 文件不存在: {txt_file}")
        return False

    with open(txt_file, "r") as f:
        lines = f.readlines()

    with open(csv_file, "w") as f:
        # ===== GyroFlow 标准头 =====
        f.write("GYROFLOW IMU LOG\n")
        f.write("version,1.3\n")
        f.write("id,custom_logger_name\n")
        f.write("orientation,YxZ\n")
        f.write("note,development_test\n")
        f.write("fwversion,FIRMWARE_0.1.0\n")
        f.write("timestamp,1644159993\n")
        f.write("vendor,potatocam\n")
        f.write(f"videofilename,{name}.mp4\n")
        f.write("lensprofile,potatocam/potatocam_mark1_prime_7_5mm_4k\n")
        f.write("lens_info,wide\n")
        f.write("frame_readout_time,15.23\n")
        f.write("frame_readout_direction,0\n")
        f.write(f"tscale,{tscale}\n")
        f.write(f"gscale,{gscale}\n")
        f.write(f"ascale,{ascale}\n")
        f.write("t,gx,gy,gz,ax,ay,az\n")

        for line in lines:
            line = line.strip()
            if line:
                f.write(line + "\n")

    print(f"✅ IMU 转换完成: {csv_file}")
    return True


def convert_h264_to_mp4(name):
    """
    video/<name>.bin -> video/<name>.mp4
    """
    input_file = os.path.join(VIDEO_DIR, name + ".bin")
    output_file = os.path.join(VIDEO_DIR, name + ".mp4")

    if not os.path.exists(input_file):
        print(f"❌ 视频文件不存在: {input_file}")
        return False

    cmd = [
        "ffmpeg",
        "-y",
        "-i", input_file,
        "-c", "copy",
        output_file
    ]

    try:
        subprocess.run(
            cmd,
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )
        print(f"✅ 视频转换完成: {output_file}")
        return True
    except subprocess.CalledProcessError:
        print(f"❌ 视频转换失败: {name}")
        return False


def convert_one(name):
    """
    单文件一键转换
    """
    print(f"\n========== 转换 {name} ==========")
    imu_ok = txt_to_gyroflow_csv(name, tscale_val, gscale_val, ascale_val)
    video_ok = convert_h264_to_mp4(name)

    if imu_ok and video_ok:
        print(f"🎉 {name} 转换完成")
    else:
        print(f"⚠️ {name} 转换未完全成功")


def convert_all():
    """
    批量转换 imu/ 和 video/ 目录下所有同名文件
    """
    if not os.path.exists(IMU_DIR) or not os.path.exists(VIDEO_DIR):
        print("❌ 缺少 imu/ 或 video/ 目录")
        return

    imu_names = {
        os.path.splitext(f)[0]
        for f in os.listdir(IMU_DIR)
        if f.endswith(".txt")
    }

    video_names = {
        os.path.splitext(f)[0]
        for f in os.listdir(VIDEO_DIR)
        if f.endswith(".bin")
    }

    common_names = sorted(imu_names & video_names)

    if not common_names:
        print("❌ 没有发现可转换的文件对")
        return

    print(f"🔍 发现 {len(common_names)} 组可转换文件")

    for name in common_names:
        convert_one(name)






if __name__ == "__main__":
    convert_all()  # 批量转换
    # convert_one(one_file_name) # 单个转换
