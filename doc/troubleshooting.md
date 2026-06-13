# 问题排查记录

> 记录项目开发中遇到的难题、分析过程和解决方案，方便复盘。

---

## 1. 录像停止时 Segfault 崩溃（2026-05-28）

### 现象

程序运行一段时间后，停止录像时发生 Segmentation fault 崩溃。Core dump 文件名：`core.QThread.977`。

### Core Dump 分析

通过 core dump 还原的崩溃调用栈：

```
Thread 1 (mp4muxerThread):
#0  0x0000007f9d1409a0 in mov_write_stbl_tag ()
   from libavformat.so.58
#1  0x0000007f9d142abc in mov_write_moov_tag ()
   from libavformat.so.58
#2  0x0000007f9d146edc in mov_write_trailer ()
   from libavformat.so.58
#3  0x0000007f9d1606cc in av_write_trailer ()
   from libavformat.so.58
#4  0x0000000000410410 in Mp4Muxer::closeMp4Mux (this=0x3f3fe8d0)
    at ../mp4muxer.cpp:391
```

崩溃发生在 FFmpeg 内部的 `mov_write_stbl_tag()` 函数，该函数负责写 MP4 容器的 sample table（stbl box）。

### 崩溃点代码

`mp4muxer.cpp:381-405`：

```cpp
void Mp4Muxer::closeMp4Mux()
{
    if (!m_outFmt) {
        return;
    }
    if (m_headerWritten) {
        flushAudioEncoder();                    // 387: 先 flush 音频编码器
    }

    if (m_headerWritten && m_outFmt->pb) {
        int ret = av_write_trailer(m_outFmt);   // 391: 写 MP4 trailer 时崩溃
        ...
    }
    ...
}
```

### 信号连接关系

```cpp
// widget.cpp:240-243
connect(cameraManager, &CameraManager::videoFinishedToMP4,
        audioManager, &AudioManager::stopCapture, Qt::DirectConnection);
connect(cameraManager, &CameraManager::videoFinishedToMP4,
        mp4Muxer, &Mp4Muxer::closeMp4Mux);  // 默认 QueuedConnection
connect(videoEncoder, &VideoEncoder::sigVideo2Mp4Mux,
        mp4Muxer, &Mp4Muxer::writeVideoToMp4);
connect(audioManager, &AudioManager::capFinishedToAudio,
        mp4Muxer, &Mp4Muxer::writeAudioToMp4);
```

### 原因分析

**GDB Core Dump 分析**（`gdb-multiarch` + `set sysroot`）：

崩溃线程：Thread 1 (LWP 995) = mp4muxerThread

```
#0  mov_write_stbl_tag+2928  → ldr w2, [x2, #4]  (x22=0xffffffff, 无效索引)
#1  mov_write_moov_tag
#2  mov_write_trailer
#3  av_write_trailer
#4  Mp4Muxer::closeMp4Mux (mp4muxer.cpp:391)
```

**根因：视频 PTS 重复为 0，导致 av_write_trailer 段错误**

复现条件：音视频同时写 MP4 + 每次开机后第一次运行程序。

打印视频 PTS 发现前两帧都是 0：

```
Video PTS = 0
Video PTS = 0
Video PTS = 33330
Video PTS = 66710
...
```

开机后第一次录像时，硬件时间戳初始化有延迟，前两帧拿到相同的 PTS 值。PTS 未严格递增，`av_write_trailer` 内部处理 stbl（sample table）时遇到重复 PTS 导致段错误。

### 修复

`writeVideoToMp4` 中增加 PTS 递增检查，跳过 PTS 未递增的帧：

```cpp
int64_t deltaUs = static_cast<int64_t>(pack->u64PTS - m_videoPtsBaseUs);
if (deltaUs < 0) deltaUs = 0;
if (deltaUs <= m_lastVideoPtsUs) {
    // PTS 未递增，跳过此帧避免时间戳回退
    continue;
}
m_lastVideoPtsUs = deltaUs;
```

### 当前状态

已解决。

---

## 2. 录像期间画面卡住（2026-05-30）

### 现象

录像期间画面突然卡住，进程仍在运行（CPU 70%），但 UI 不再刷新。

### 调试方法

使用 strace 跟踪系统调用：

```bash
# 板子上：从头启动并跟踪
strace -f /root/ai-cam-ui-widget 2>&1 | tee /tmp/strace.log

# 复现卡住后 Ctrl+C 停掉 strace，将 strace.log 拷贝到宿主机分析
```

### 分析过程

1. 统计所有线程（PID）的最后活动时间，发现 PID 887（摄像头线程）在报错后进入 idle
2. 查看 PID 887 最后的有意义系统调用：

```
VIDIOC_S_FMT failed: EBUSY (Device or resource busy)
RTDeviceV4L2 {ispInitFormat:613} /dev/video0: failed to set format(capType=1, NV12, w=3264, h=2448)
RTNodeSourceV4L2 {openDev:300} failed to open task node for video/v4l2
RTNodeSourceV4L2 {process:190} open isp v4l2 err.
RTSchedulerQueue {runTaskNode:157} node 0:rkisp had an error
```

3. PID 888 也遇到类似问题：`VIDIOC_S_FMT(1920x1080) → EBUSY`

### 根因

ISP V4L2 设备被占用（EBUSY），摄像头线程打开失败后进入 idle 循环，不再出帧 → 视频编码器和 UI 都停止更新 → 画面卡住。

3264x2448 是拍照分辨率（不是录像的 1920x1080），但用户确认未触发拍照操作。可能是摄像头线程内部的定时重连或 ISP 异常恢复逻辑触发了设备重新初始化，与正在运行的视频流产生资源冲突。需进一步排查 CameraManager 中是否有自动重连/重初始化机制。

### 当前状态

待修复。

---

<!-- 后续问题在此处继续添加 -->
