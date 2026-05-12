---
name: cam-ui-widget-project
description: 当需要接手或修改 rk-camera-display/cam-ui-widget 这个 Qt/Rockchip 项目时使用本技能，尤其适用于摄像头预览、H.264/MP4 录像、MP4 解封装播放、VDEC 视频显示、AI/AO 音频采集播放、FFmpeg 封装解封装和编解码、音视频同步、以及本项目中 MP4 播放相关 bug 修复。
---

# cam-ui-widget 项目接手指南

## 用途

这份 skill 是 `/home/lzp/rk-camera-display/cam-ui-widget` 的项目交接文档。

在修改以下内容前，先阅读本文件：

- 摄像头采集和预览
- 视频编码、解码
- MP4 封装和解封装
- 音频采集和播放
- Qt 线程和信号槽
- 音视频同步
- 本项目新增的 MP4 解封装播放、音频播放、同步和二次播放修复

这个应用是一个基于 Qt Widgets 的 Rockchip 媒体链路 UI，主要能力包括：

- VI 摄像头实时预览
- 拍照
- H.264 视频 + AAC 音频录制到 MP4
- 视频相册和 MP4 播放
- RK VDEC 硬件 H.264 解码
- RK AI 音频采集
- FFmpeg 音频解码 + RK AO 音频播放

## 构建方式

当前有效构建目录：

```bash
cd /home/lzp/rk-camera-display/cam-ui-widget/build-aarch64
make -j2
```

项目使用 qmake：

- 工程文件：`ai-cam-ui-widget.pro`
- 输出程序：`build-aarch64/ai-cam-ui-widget`
- 交叉编译工具链路径已经写在 `build-aarch64/Makefile` 中

当前构建中 `videodecget.cpp` 可能会有一个关于 `FRAME_FLAG_SNAP_END` 的 signed/unsigned warning，这是既有 warning，和 MP4/音频改动无关。

## 关键文件

- `widget.cpp/.h`：UI 初始化、线程创建、信号槽连接、相册/视频列表逻辑。
- `cameracapture.cpp/.h`：VI 摄像头采集循环、预览帧转换、录像路径生成、首帧缩略图保存。
- `savevideo.cpp/.h`：把采集帧送入 RK VENC 编码 H.264，并把编码流发给 MP4 封装器。
- `mp4muxdemux.cpp/.h`：MP4 封装主逻辑；也保留了一个早期的“MP4 解出完整 H.264 文件”辅助接口。
- `videodecsend.cpp/.h`：播放输入侧；流式解封装 MP4，把视频包送 VDEC，把音频包发给 `AudioManager`。
- `videodecget.cpp/.h`：播放输出侧；轮询 VDEC 解码帧，用 RGA 转 BGR，再发给 UI。
- `audiocapture.cpp/.h`：现在类名是 `AudioManager`，同时负责 AI 音频采集和 AO 音频播放。
- `rk_mpi.cpp/.h`：本地 RK MPI 初始化封装，例如 `vdec_init`、`venc_init`。
- `widget.ui`：Qt Designer UI 文件。

## 线程模型

`Widget` 创建 worker 对象，并分别移动到独立 QThread：

- `CameraCapture` -> `cameracapThread`
- `SavePicture` -> `savepictureThread`
- `SaveVideo` -> `savevideoThread`
- `KeyControl` -> `keycontrolThread`
- `VideoDecSend` -> `videodecsendThread`
- `VideoDecGet` -> `videodecgetThread`
- `Mp4MuxDemux` -> `mp4muxThread`
- `AudioManager` -> `audiocapThread`

注意：

- 不要在 Qt worker slot 中做无界阻塞。
- 一个 worker 的 slot 被阻塞后，发给同一个对象的 queued signals 都会排队，无法执行。
- 这正是“第一次播放有声音，退出后第二次播放没声音”的根因之一：早期 `AudioManager::stopPlayback()` 里 `RK_MPI_AO_WaitEos(..., -1)` 可能无限等待，导致后续 `startPlayback` 执行不到。

## 录像流程

1. `KeyControl::keyToStartvideo` 触发录像状态和 `CameraCapture::startVideo`。
2. `CameraCapture::startCapture` 从程序启动后就一直在采集线程运行。
3. 开始录像后的第一帧到达时，`cameracapture.cpp` 创建：
   - 调试/兼容用裸流路径：`/userdata/video/<timestamp>.bin`
   - MP4 路径：`/userdata/video/<timestamp>.mp4`
   - 首帧缩略图：`/userdata/firstFrame/<timestamp>.jpg`
4. UI 视频列表保存的是 MP4 路径，不再是旧的 `.bin` 路径。
5. `CameraCapture::sigVideoMp4Init(savePath_mp4)` 同时触发：
   - `AudioManager::startCapture`
   - `Mp4MuxDemux::initMp4Mux`
6. 每一帧录像图像送到 `SaveVideo::do_save_video`。
7. `SaveVideo` 把图像送 RK VENC 编码，并发出 `sigVideo2Mp4Mux(&stFrame)`。
8. `Mp4MuxDemux::writeVideoToMp4` 把 H.264 写入 MP4。
9. `AudioManager::startCapture` 从 RK AI 取 PCM，并通过 `capFinishedToAudio` 发给 MP4 封装器。
10. `Mp4MuxDemux::writeAudioToMp4` 把 PCM 编成 AAC，再交织写入 MP4。
11. 停止录像时，`CameraCapture::videoFinishedToMP4` 先停止音频采集，再关闭 MP4 封装器。

## MP4 封装总览

`Mp4MuxDemux` 使用 FFmpeg 写 MP4。

视频侧：

- RK VENC 输出 Annex-B 格式 H.264。
- MP4 需要 AVCC 格式 H.264 sample，以及 `avcC` extradata。
- `findAnnexbStartCode`、SPS/PPS 提取、`buildAvccFromSpsPps`、`annexbToAvcc` 负责格式转换。
- MP4 header 会延迟到拿到 SPS/PPS 后再写。

音频侧：

- RK AI 采集得到 s16 PCM。
- AAC 编码器通常需要 planar float，例如 `fltp`。
- `SwrContext` 把输入 s16 PCM 转成 AAC 编码器需要的采样格式。
- MP4 header 写出前到达的音频先放入 `m_pendingAudioChunks`。
- 视频 SPS/PPS 到达并写完 MP4 header 后，再 flush pending audio。

## 录像视频格式转换：Video To MP4

录像视频链路跨越多个格式：

1. 摄像头 VI 输出 YUV semi-planar 图像。
2. `cameracapture.cpp` 对同一帧做不同用途的转换：
   - 预览/UI 路径：YUV -> RGB/BGR，并 resize 到 UI 尺寸。
   - 录像路径：YUV -> BGR，作为 `SaveVideo` 输入。
3. `SaveVideo::do_save_video` 把 BGR 图像拷贝到 RK VENC 输入内存。
4. RK VENC 编码输出 H.264 Annex-B。
5. `SaveVideo` 发出 `sigVideo2Mp4Mux(&stFrame)`。
6. `Mp4MuxDemux::writeVideoToMp4` 收到 Annex-B 编码包。
7. 写 MP4 时：
   - 从 Annex-B 中提取 SPS/PPS NAL。
   - 用 `buildAvccFromSpsPps` 把 SPS/PPS 组装成 `avcC` extradata。
   - 用 `annexbToAvcc` 把实际视频 sample 从 Annex-B 起始码格式转成 AVCC 长度前缀格式。
8. FFmpeg 把 AVCC 格式 packet 写入 MP4 视频轨。

Annex-B 和 AVCC 的区别：

- Annex-B：每个 NAL 前面是起始码，例如 `00 00 01` 或 `00 00 00 01`。
- AVCC：每个 NAL 前面是长度字段，不使用起始码。
- MP4 还会把解码初始化信息单独放在 `AVCodecParameters::extradata`，格式为 `avcC`。
- RK VENC/VDEC 更适合 Annex-B；MP4 容器要求 AVCC。

`mp4muxdemux.cpp` 中重要函数：

- `findAnnexbStartCode`：扫描 H.264 裸流中的 Annex-B 起始码。
- `buildAvccFromSpsPps`：用 SPS/PPS 构造 MP4 需要的 `avcC` extradata。
- `annexbToAvcc`：把视频 sample 从 Annex-B 转成 AVCC。
- `addSpsPpsAndWriteHeader`：创建 MP4 视频轨，并在 SPS/PPS 准备好后写 MP4 header。
- `writeVideoToMp4`：提取 SPS/PPS，转换 packet，计算时间戳，写入 MP4。

## 录像音频格式转换：Audio To MP4

录像音频链路：

1. `AudioManager::startCapture` 从 RK AI 读取音频帧。
2. RK AI 输出 PCM，目前按 signed 16-bit PCM，也就是 `s16` 处理。
3. `AudioManager` 发出 `capFinishedToAudio(QByteArray pcmData, uint64_t captureTsUs)`。
4. `Mp4MuxDemux::writeAudioToMp4` 收到 PCM chunk。
5. 如果 MP4 header 还没有写，PCM chunk 先缓存到 `m_pendingAudioChunks`。
6. 视频 SPS/PPS 到达并写完 MP4 header 后，pending audio 会被 flush。
7. PCM chunk 会累积到 `m_audioPcmCache`，直到足够填满一个 AAC 编码帧。
8. `swr_convert` 把输入 s16 PCM 转成 AAC 编码器需要的采样格式，常见为 `fltp`。
9. `avcodec_send_frame` / `avcodec_receive_packet` 编码得到 AAC packet。
10. AAC packet 设置时间戳后，用 `av_interleaved_write_frame` 写入 MP4。

`mp4muxdemux.cpp` 中重要函数：

- `initAudioEncoder`：创建 AAC 编码器、音频流、输入 layout 和 resampler。
- `writeAudioToMp4`：MP4 header 前缓存音频，header 后处理音频。
- `processAudioChunk`：把任意大小 PCM chunk 整理成 AAC 编码器需要的 frame。
- `writeAudioFrame`：接收编码后的 AAC packet 并写入 MP4。
- `flushAudioEncoder`：末尾 PCM 不足一帧时补零，flush 编码器延迟包，避免丢尾音。

## MP4 录像音视频同步机制

录像同步基于采集时间戳，单位按微秒处理。

`Mp4MuxDemux` 中相关状态：

- `m_syncBaseInited`
- `m_syncBaseUs`
- `m_videoPtsInited`
- `m_videoPtsBaseUs`
- `m_audioPtsInited`
- `m_audioNextPts`

工作方式：

1. 第一份到达 `Mp4MuxDemux` 的音频或视频时间戳会初始化 `m_syncBaseUs`。
2. 音频和视频都减去同一个基准，得到相对“本次录像开始”的时间戳。
3. 视频：
   - `writeVideoToMp4` 调用 `ensureSyncBase(pack->u64PTS)`。
   - 第一帧视频初始化 `m_videoPtsBaseUs = m_syncBaseUs`。
   - packet 的 `pts/dts` = `pack->u64PTS - m_videoPtsBaseUs`，小于 0 时钳成 0。
   - 视频流 time_base 是微秒：`AVRational{1, 1000000}`。
4. 音频：
   - `processAudioChunk` 调用 `ensureSyncBase(captureTsUs)`。
   - 第一块音频计算 `deltaUs = captureTsUs - m_syncBaseUs`。
   - `deltaUs` 被 rescale 到音频编码器 time_base。
   - 之后音频 PTS 不再依赖线程到达时间，而是每编码一帧增加 `m_audioFrame->nb_samples`。

为什么音频 PTS 用 sample count 推进：

- Qt 信号、线程调度、AI 获取帧都有抖动。
- PCM 本质是连续采样流。
- 只要起点对齐共享基准，后续按样本数推进比按回调到达时间更稳定。

为什么 MP4 header 前要缓存音频：

- MP4 header 需要 H.264 视频轨的 SPS/PPS。
- 音频可能比第一份带 SPS/PPS 的视频 packet 更早到。
- `m_pendingAudioChunks` 保存这些早到的音频，避免录像开头音频丢失。

常见同步风险：

- 如果音频和视频时间戳不是同一时钟域，`m_syncBaseUs` 会对齐错误。
- 如果 VENC `pack->u64PTS` 不是微秒，视频 PTS 计算要调整。
- 如果音频采集格式变化，需要同步更新 `m_audioInputChannels`、`m_audioInputBytesPerSample` 和 resampler 配置。
- 如果开始录像后很久才拿到 SPS/PPS，pending audio 会在 header 写出前持续累积。

## MP4 解封装辅助函数说明

`Mp4MuxDemux::demuxMp4ToH264` 是早期实现中留下的辅助接口。

它的作用：

- 打开 MP4 文件。
- 使用 FFmpeg 的 `h264_mp4toannexb` bitstream filter。
- 把 MP4 中的视频流完整写成一个 Annex-B `.h264` 文件。

正常播放已经不再使用这个函数，因为它会导致启动慢并产生临时文件。当前正常播放使用 `VideoDecSend::do_send_mp4_frame` 做流式解封装。

## 播放流程

点击视频列表中的一个视频：

1. `Widget::VideoListItemClicked` 调用 `vdec_init(1920, 1080)`。
2. UI 切换到视频播放页。
3. 发出 `WidgetToStartDecVideo(path)`。
4. 该信号同时连接到：
   - `VideoDecSend::do_send_frame`
   - `VideoDecGet::do_get_frame`

视频路径：

- `VideoDecSend::do_send_frame` 检测到 `.mp4`。
- 调用 `do_send_mp4_frame`。
- FFmpeg 打开 MP4，并查找 best video/audio stream。
- 当前要求视频流是 H.264。
- 视频 packet 使用 `h264_mp4toannexb` 从 MP4/AVCC 转成 Annex-B。
- 转换后的 packet 直接通过 `RK_MPI_VDEC_SendStream` 送入 RK VDEC。
- 不再写 `/tmp` 临时 H.264 文件。

音频路径：

- `VideoDecSend` 发出：
  - `audioStreamStarted(codecId, sampleRate, channels, extradata)`
  - `audioPacketReady(packetData, pts)`
  - `audioStreamFinished()`
- `AudioManager` 在自己的线程接收这些信号。
- `AudioManager::startPlayback` 根据 MP4 音频流参数打开 FFmpeg 解码器。
- `AudioManager::playEncodedAudioPacket` 解码压缩音频 packet。
- 解码后的音频通过 `swr_convert` 转成 s16 PCM。
- PCM 通过 `RK_MPI_AO_SendFrame` 送入 RK AO 播放。

VDEC 输出路径：

- `VideoDecGet::do_get_frame` 轮询 `RK_MPI_VDEC_GetFd(0)`。
- 调用 `RK_MPI_VDEC_GetFrame` 获取解码帧。
- 用 RGA 把 decoded YUV 转成 `cv::Mat video_frame_rgb(368, 552, CV_8UC3)`。
- 发出 `VideodecToUI(video_frame_rgb)`。
- UI 在 `Widget::doShowVideo` 中显示。

## MP4 播放音视频同步机制

当前 MP4 播放以 `VideoDecSend` 的解封装线程作为 packet 分发时钟。

为什么这么做：

- 早期实现中，解封装读到 packet 就立即发出去。
- 音频、视频进入不同缓冲后各自运行，容易产生音频断续和音画漂移。
- `videodecget.cpp` 原来还有固定 `usleep(20000)`，等于强行假设 50 FPS，会和 MP4 真实时间线冲突。

当前行为：

- `VideoDecSend::packetTimeUs` 取 packet 的 `pts`，没有则取 `dts`。
- 该时间戳按对应 stream 的 `time_base` 转成微秒。
- `VideoDecSend::do_send_mp4_frame` 用 `av_gettime_relative` 建立播放起始墙钟。
- 第一份有效 packet 时间戳成为 `firstMediaUs`。
- 每个后续 packet 的目标发送时间：
  - `targetUs = playStartUs + (pktMediaUs - firstMediaUs)`
- 解封装线程 sleep 到目标时间，再把 packet 发给音频或视频。
- 视频 packet 的 PTS 也写入 `VDEC_STREAM_S::u64PTS`。
- `VideoDecGet` 不再使用固定 `usleep(20000)` 控帧。

这样做的效果：

- 音频和视频 packet 按同一个 MP4 时间线离开 demuxer。
- 下游 AO、VDEC、UI 队列不会被瞬间塞满。
- 比“尽快读完整个文件”更接近真实播放。

这不是完整播放器主时钟：

- 不会丢弃过晚的视频帧。
- 不会拉伸或压缩音频。
- 不会查询真实 AO 硬件延迟。
- 不能完全消除硬件输出缓冲带来的固定偏移。

如果后续需要更精确同步：

- 推荐以音频硬件时钟作为 master clock。
- 在 `AudioManager` 里累计实际播放样本数，估算 audio clock。
- 在 `VideoDecGet` 根据视频帧 PTS 和 audio clock 决定延迟显示或丢帧。
- 如果实测存在固定 AO 延迟，再加可调 offset。

## 播放视频格式转换：MP4 To Video Display

播放视频链路：

1. `VideoDecSend::do_send_frame` 检测 `.mp4`。
2. `VideoDecSend::do_send_mp4_frame` 用 FFmpeg 打开 MP4。
3. 选择 best video stream，并确认 codec 是 H.264。
4. MP4 中的视频 packet 是 AVCC 形式。
5. FFmpeg bitstream filter `h264_mp4toannexb` 把 AVCC sample 转成 Annex-B。
6. `send_stream_data` 把 Annex-B 数据包装进 RK MB block。
7. `RK_MPI_VDEC_SendStream` 把数据送入 RK VDEC。
8. `VideoDecGet::do_get_frame` 从 RK VDEC 取 decoded frame。
9. RGA 把 decoded YUV（`RK_FORMAT_YCrCb_420_SP`）转成 BGR888。
10. UI 收到 `cv::Mat` 并显示。

为什么需要 bitstream filter：

- MP4 中 H.264 NAL 是长度前缀，codec metadata 在 extradata。
- RK VDEC 需要更像裸流的 Annex-B 起始码形式，并且 SPS/PPS 要能被解码器拿到。
- `h264_mp4toannexb` 会完成必要的转换和参数注入。

不要在正常播放中使用旧路径：

- MP4 -> 完整临时 `.h264` 文件 -> fread -> VDEC

当前正确路径：

- MP4 -> packet demux -> `h264_mp4toannexb` -> 立即送 VDEC

## 播放音频格式转换：MP4 To Audio Output

播放音频链路：

1. `VideoDecSend::do_send_mp4_frame` 选择 best audio stream。
2. 发出 `audioStreamStarted(codecId, sampleRate, channels, extradata)`。
3. 每个 MP4 音频 packet 发出 `audioPacketReady(packetData, pts)`。
4. `AudioManager::startPlayback` 用 codec id 和 extradata 创建 FFmpeg 解码器。
5. `AudioManager::playEncodedAudioPacket` 把压缩 packet 送入解码器。
6. 解码器输出可能是 planar float、interleaved s16 或其他格式。
7. `swr_convert` 把 decoded frame 转成 signed 16-bit PCM，即 `AV_SAMPLE_FMT_S16`。
8. `sendPcmToAo` 把 PCM 包装进 RK MB block，并调用 `RK_MPI_AO_SendFrame` 播放。

关键区别：

- MP4 中的音频 packet 是压缩数据，例如 AAC。
- RK AO 播放的是 PCM。
- 因此不能把 MP4 音频 packet 直接送 AO，必须先解码。

当前 AO 期望：

- bit width：16-bit
- sample format：signed 16-bit PCM
- channels：mono 或 stereo
- sample rate：优先使用 MP4 音频流 sample rate；无效时回退到采集配置

## AudioManager 说明

文件仍然叫 `audiocapture.cpp/.h`，但类名已经改为 `AudioManager`。

兼容：

- 头文件保留 `using AudioCapture = AudioManager;`，兼容旧引用。

采集侧：

- `startCapture`
- `stopCapture`
- `capFinishedToAudio(QByteArray pcmData, uint64_t captureTsUs)`
- 使用 RK AI API。

播放侧：

- `startPlayback(int codecId, int sampleRate, int channels, QByteArray extradata)`
- `playEncodedAudioPacket(QByteArray packetData, qint64 pts)`
- `stopPlayback`
- 使用 FFmpeg decode + swresample + RK AO。

AO 初始化/播放/关闭主要使用：

- `RK_MPI_AO_SetPubAttr`
- `RK_MPI_AO_Enable`
- `RK_MPI_AO_EnableChn`
- `RK_MPI_AO_EnableReSmp`
- `RK_MPI_AO_SendFrame`
- `RK_MPI_AO_ClearChnBuf`
- `RK_MPI_AO_DisableReSmp`
- `RK_MPI_AO_DisableChn`
- `RK_MPI_AO_Disable`

开发时参考过的 AO 示例：

- `/home/lzp/rk-camera-display/rockit/mpi/example/mod/test_mpi_ao.cpp`

## MP4/音频相关 bug 修复记录

### MP4 播放启动慢

问题：

- 第一版播放会先把整个 MP4 解封装成 `/tmp/*.h264`，再播放。
- 视频越长，启动越慢，还会产生不必要的写盘。

修复：

- `VideoDecSend::do_send_mp4_frame` 改成 packet-by-packet 流式解封装。
- 视频 packet 转 Annex-B 后直接送 VDEC。
- 不再写临时 H.264 文件。

### 播放没有音频

问题：

- MP4 播放时忽略了音频流。

修复：

- `VideoDecSend` 查找 MP4 audio stream，并发出压缩音频 packet。
- `AudioManager` 使用 FFmpeg 解码音频 packet，再把 PCM 送 RK AO 播放。

### 音频不连续、音画不同步

问题：

- 解封装读到 packet 就立即发出。
- 视频显示端还有固定 `usleep(20000)`。
- 音频和视频使用了不同的节奏，容易漂移。

修复：

- MP4 demux 根据 packet PTS/DTS 控制发包节奏。
- 移除 `VideoDecGet` 中固定 sleep。

### 第二次播放没有声音

问题：

- `AudioManager::stopPlayback` 里曾经使用 `RK_MPI_AO_WaitEos(..., -1)`。
- 用户中途退出播放时，音频线程可能无限等待。
- 后续 queued `startPlayback` 信号无法执行，导致第二次播放无声。

修复：

- `stopPlayback` 立即设置 `isPlaying = 0`。
- AO wait 改成 `kPlaybackStopWaitMs = 100` 的有限等待。
- 停止和 deinit 时清空 AO channel buffer。

## 调试建议

- 视频有、音频没有：检查是否发出了 `audioStreamStarted`，以及 `AudioManager::startPlayback` 是否被前一个阻塞 slot 卡住。
- 第一次有声音、第二次没声音：优先检查 AO stop/deinit、`RK_MPI_AO_WaitEos`、queued signal 顺序。
- 播放卡顿：不要一次性快速推完整个 MP4，必须保留基于 PTS/DTS 的发包节流。
- 音画固定偏移：优先加可配置 offset，不要恢复固定 sleep。
- 音画逐渐漂移：检查 stream time_base、PTS 转换、AO/UI 缓冲。
- 新录出来的 MP4 没音频：检查 AI capture -> `capFinishedToAudio` -> `writeAudioToMp4`。
- MP4 播不了：确认视频 codec 是 H.264；当前 VDEC 播放路径只支持 H.264 MP4。
- 改类名或信号签名时，要同步检查 qmake/moc 和 `widget.cpp` 的连接。

## 后续编码约定

- 改动尽量小而集中。这个项目强依赖硬件，宽泛重构风险高。
- RK media 对象生命周期要明确：enable、enable channel、enable resample；关闭时反向 disable，并清 buffer。
- Qt worker slot 中避免无限等待。
- H.264/MP4：
  - RK VENC/VDEC 侧偏 Annex-B。
  - MP4 sample/extradata 侧偏 AVCC。
  - 优先使用 FFmpeg bitstream filter 或已有 helper，不要手写脆弱的字节替换。
- 音频：
  - MP4 packet 是压缩数据，AO 需要 PCM。
  - AO frame 要保证 bit width、sample rate、channel mode 正确。
  - 解码输出格式和 AO 不一致时，用 `swr_convert`。

## 当前已知限制

- 播放已有时间戳节流，但还不是完整 master-clock 播放器。
- AO 设备缓冲可能引入固定延迟；应在硬件实测后用 offset 补偿。
- UI 显示仍通过 Qt signal 传 `cv::Mat`，UI 忙时可能影响流畅度。
- `VideoDecSend::do_send_frame` 仍保留 `.bin`/裸 H.264 fallback，但正常相册播放应使用 `.mp4`。
