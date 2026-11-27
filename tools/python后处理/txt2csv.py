import os


def txt_to_gyrosflow_csv(txt_file, tscale=0.001, gscale=0.00122173047, ascale=0.00048828125):
    """
    将 TXT 格式的 IMU 数据转换为 GyroFlow CSV 格式

    txt_file: 输入的 TXT 文件路径
    tscale, gscale, ascale: 可配置参数
    """
    # 根据输入 TXT 文件名生成 CSV 文件名
    base_name = os.path.basename(txt_file)
    name_no_ext = os.path.splitext(base_name)[0]
    csv_file = os.path.join(os.path.dirname(txt_file), name_no_ext + ".csv")

    # 获取文件内容
    with open(txt_file, 'r') as f:
        lines = f.readlines()

    # 写入 CSV
    with open(csv_file, 'w') as f:
        # 写 GyroFlow 必要表头
        f.write("GYROFLOW IMU LOG\n")
        f.write("version,1.3\n")
        f.write("id,custom_logger_name\n")
        f.write("orientation,YxZ\n")
        f.write("note,development_test\n")
        f.write("fwversion,FIRMWARE_0.1.0\n")
        f.write("timestamp,1644159993\n")
        f.write("vendor,potatocam\n")
        f.write(f"videofilename,{name_no_ext}.mp4\n")  # 根据 txt 文件名生成
        f.write("lensprofile,potatocam/potatocam_mark1_prime_7_5mm_4k\n")
        f.write("lens_info,wide\n")
        f.write("frame_readout_time,15.23\n")
        f.write("frame_readout_direction,0\n")
        f.write(f"tscale,{tscale}\n")
        f.write(f"gscale,{gscale}\n")
        f.write(f"ascale,{ascale}\n")
        f.write("t,gx,gy,gz,ax,ay,az\n")

        # 写入每一行 IMU 数据
        for line in lines:
            # 清理行末换行符
            line = line.strip()
            if not line:
                continue
            # 将数据写入 CSV
            # 可以在这里加乘以 gscale, ascale，如果需要
            f.write(f"{line}\n")

    print(f"转换完成，CSV 文件已生成: {csv_file}")


# ===========================
# 示例调用
# ===========================
if __name__ == "__main__":
    # 输入 TXT 文件路径
    txt_path = "imu/20170804_090024.txt"

    # 可自行修改 tscale, gscale, ascale
    tscale_val = 0.02  # 200hz
    gscale_val = 0.01745329252 * 1000.0 / 32768.0  # π / 180
    ascale_val = 0.001 * 4000 / 32768.0

    txt_to_gyrosflow_csv(txt_path, tscale=tscale_val, gscale=gscale_val, ascale=ascale_val)
