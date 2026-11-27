import subprocess
import os

def convert_h264_to_mp4():
    # ====== 在这里内置参数 ======
    input_file = "./video/20170804_090023.bin"     # 你的 h264 文件名
    output_file = "len_config.mp4"    # 输出 mp4 文件名
    # ==========================

    if not os.path.exists(input_file):
        print(f"❌ 输入文件不存在: {input_file}")
        return

    cmd = [
        "ffmpeg",
        "-y",          # 覆盖输出
        "-i", input_file,
        "-c", "copy",  # 不重新编码，只封装
        output_file
    ]

    print("执行命令:", " ".join(cmd))

    try:
        subprocess.run(cmd, check=True)
        print(f"✅ 转换成功！输出: {output_file}")
    except subprocess.CalledProcessError:
        print("❌ 转换失败，请检查 FFmpeg 是否安装")

if __name__ == "__main__":
    convert_h264_to_mp4()
