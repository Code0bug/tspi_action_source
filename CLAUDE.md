# cam-ui-widget

Qt Widgets Rockchip 摄像头应用，支持实时预览、H.264/AAC MP4 录像、MP4 播放、硬件编解码。

## 构建

```bash
cd build-aarch64
make -j2
```

- 工程文件：`ai-cam-ui-widget.pro`（qmake）
- 输出：`build-aarch64/ai-cam-ui-widget`
- qmake 路径：`/home/lzp/linux_sdk/buildroot/output/rockchip_rk3566/host/bin/qmake`
- 交叉编译工具链路径已写在 `build-aarch64/Makefile` 中
- 如需重新生成 Makefile（例如修改 .pro 后）：
  ```bash
  cd build-aarch64
  /home/lzp/linux_sdk/buildroot/output/rockchip_rk3566/host/bin/qmake -o Makefile ../ai-cam-ui-widget.pro -spec devices/linux-buildroot-g++ CONFIG+=debug CONFIG+=qml_debug
  ```
- `videodecoder.cpp` 有一个关于 `FRAME_FLAG_SNAP_END` 的 signed/unsigned warning，是既有问题，忽略即可

## 关键文件

| 文件 | 职责 |
|------|------|
| `widget.cpp/.h` | UI 初始化、线程创建、信号槽连接、相册/视频列表 |
| `cameramanager.cpp/.h` | VI 摄像头采集、预览帧转换、录像路径、首帧缩略图 |
| `videoencoder.cpp/.h` | BGR 帧送 RK VENC 编码 H.264，发给 MP4 封装器 |
| `mp4muxer.cpp/.h` | MP4 封装/解封装主逻辑，Annex-B↔AVCC 转换 |
| `mp4demuxer.cpp/.h` | 播放输入：流式解封装 MP4，视频送 VDEC，音频发 AudioManager |
| `videodecoder.cpp/.h` | 播放输出：轮询 VDEC 解码帧，RGA 转 BGR，发给 UI |
| `audiomanager.cpp/.h` | `AudioManager` 类，RK AI 采集 + AO 播放 + FFmpeg 解码 |
| `utils/rk_mpi.cpp/.h` | RK MPI 初始化封装（`vdec_init`、`venc_init`） |
| `utils/safeQueue.h` | 线程安全队列模板（`SafeQueue<T>`） |
| `widget.ui` | Qt Designer UI |

## 线程模型

`Widget` 创建 worker 对象，移动到独立 QThread：

- `CameraManager` → `cameramanagerThread`
- `SavePicture` → `savepictureThread`
- `VideoEncoder` → `videoencoderThread`
- `KeyControl` → `keycontrolThread`
- `Mp4Demuxer` → `mp4demuxerThread`
- `VideoDecoder` → `videodecoderThread`
- `Mp4Muxer` → `mp4muxerThread`
- `AudioManager` → `audiomanagerThread`

**注意**：worker slot 中不能无界阻塞，否则 queued signal 全部排队无法执行。这是"第二次播放没声音"类 bug 的常见根因。

## 录像链路（Audio/Video → MP4）

### 录像总览

```
┌─────────────┐     ┌──────────────────┐     ┌──────────────────┐     ┌──────────────────┐
│  摄像头传感器  │────▶│   CameraManager   │────▶│    VideoEncoder      │────▶│   Mp4Muxer    │
│  (VI 硬件)   │     │  cameramanagerThread  │     │  videoencoderThread  │     │   mp4muxerThread   │
└─────────────┘     └──────────────────┘     └──────────────────┘     └──────────────────┘
┌─────────────┐     ┌──────────────────┐                                        ▲
│  麦克风传感器  │────▶│   AudioManager    │──────────── capFinishedToAudio ────────────┘
│  (AI 硬件)   │     │  audiomanagerThread   │
└─────────────┘     └──────────────────┘
```

### 视频链路：摄像头 → MP4

```
VI 硬件 (RK_FMT_YUV420SP, 1920×1080)
  │
  ▼
CameraManager::startCapture (cameramanagerThread)
  │
  ├── RGA imcvtcolor ──▶ RGB888 552×368 ──▶ capFinishedToUI ──▶ Widget::doShow (预览)
  │
  └── RGA imcvtcolor ──▶ BGR888 1920×1080
           │
           │ capFinishedToVideo(cv::Mat BGR, FILE*, PTS)  ── 跨线程信号 ──▶
           ▼
VideoEncoder::do_save_video (videoencoderThread)
  │  memcpy BGR → DMA buf
  │  RK_MPI_VENC_SendFrame (BGR888 → H.264 硬件编码)
  │  RK_MPI_VENC_GetStream (得到 H.264 Annex-B 码流)
  │
  ├── fwrite → .bin 文件 (裸 H.264 Annex-B，调试用)
  │
  └── emit sigVideo2Mp4Mux(VENC_STREAM_S*)  ── 跨线程信号 ──▶
           │
           ▼
Mp4Muxer::writeVideoToMp4 (mp4muxerThread)
  │  从 Annex-B 提取 SPS/PPS (首个关键帧)
  │  buildAvccFromSpsPps → 生成 avcC (AVCDecoderConfigurationRecord)
  │  addMp4Stream + avformat_write_header (写 MP4 moov)
  │  annexbToAvcc (起始码 → 4字节长度前缀)
  │  av_interleaved_write_frame (AVCC 格式 H.264 写入 MP4)
  │
  ▼
MP4 文件 (.mp4)
```

#### 视频格式转换：Annex-B vs AVCC

- RK VENC 输出 Annex-B 格式 H.264（`00 00 01` 起始码）
- MP4 容器要求 AVCC 格式（4字节长度前缀 + `avcC` extradata）
- `findAnnexbStartCode`：扫描 Annex-B 起始码
- `buildAvccFromSpsPps`：用 SPS/PPS 构造 `avcC` extradata
- `annexbToAvcc`：将视频 sample 从 Annex-B 转成 AVCC
- MP4 header 延迟到拿到 SPS/PPS 后才写；之前到达的音频缓存在 `m_pendingAudioChunks`

### 音频链路：麦克风 → MP4

```
AI 硬件 (PCM s16, 2ch, 16kHz)
  │
  ▼
AudioManager::startCapture (audiomanagerThread)
  │  RK_MPI_AI_GetFrame (阻塞读取一帧 PCM)
  │
  │ capFinishedToAudio(QByteArray PCM, timestamp)  ── 跨线程信号 ──▶
  ▼
Mp4Muxer::writeAudioToMp4 (mp4muxerThread)
  │
  ├─ 若 MP4 header 未写 → 缓存到 m_pendingAudioChunks
  │
  └─ 若 MP4 header 已写 → processAudioChunk
       │  PCM 累积到 m_audioPcmCache
       │  凑够 1024 samples (4096 bytes) → swr_convert (s16 → fltp)
       │  avcodec_send_frame / avcodec_receive_packet (AAC 编码, 64kbps)
       │  av_interleaved_write_frame (AAC 写入 MP4，与视频交织)
       │
       ▼
  MP4 文件 (.mp4)
```

### 录像音视频同步

- 共享 `m_syncBaseUs` 基准，音视频都减去同一基准
- 视频 PTS = `pack->u64PTS - m_videoPtsBaseUs`，time_base 微秒
- 音频 PTS 按 sample count 推进（比用回调到达时间更稳定）

### 录像启动/停止

**启动**：`KeyControl` 触发 → `CameraManager::startVideo` → 第一帧到达时创建 `.bin`/`.mp4`/`.jpg` → `sigVideoMp4Init` 同时触发 `AudioManager::startCapture` + `Mp4Muxer::initMp4Mux`

**停止**：`CameraManager::stopVideo` → `videoFinishedToMP4` → 先停音频采集（`AudioManager::stopCapture`），再关 MP4 封装器（`Mp4Muxer::closeMp4Mux`：flush 音频编码器、写 MP4 trailer、关闭文件）

---

## 播放链路（MP4 → Audio/Video 输出）

### 播放总览

```
                                    ┌──────────────────────┐
                               ┌───▶│    VideoDecoder        │───▶ 屏幕显示
                               │    │   videodecoderThread    │
MP4 文件 ──▶ Mp4Demuxer ─────┤    └──────────────────────┘
            mp4demuxerThread │    ┌──────────────────────┐
                               └───▶│    AudioManager       │───▶ 扬声器播放
                                    │   audiomanagerThread       │
                                    └──────────────────────┘
```

### 视频链路：MP4 → 屏幕

```
MP4 文件
  │
  ▼
Mp4Demuxer::do_send_mp4_frame (mp4demuxerThread)
  │  avformat_open_input / av_read_frame (libavformat 解封装)
  │  选择 video stream (必须是 H.264)
  │
  │  对每个视频 packet：
  │    av_bsf (h264_mp4toannexb) ── AVCC 转 Annex-B
  │    send_stream_data() ── RK_MPI_VDEC_SendStream(chn=0) 送入硬件解码器
  │    (PTS 墙钟节流：av_gettime_relative 控制发包节奏)
  │
  ▼
Rockchip VDEC 硬件解码
  │
  ▼
VideoDecoder::do_get_frame (videodecoderThread)
  │  poll() 等待解码帧就绪
  │  RK_MPI_VDEC_GetFrame → YCrCb_420_SP (NV21)
  │  RGA imresize + imcvtcolor → BGR_888 (552×368)
  │  emit VideodecToUI(cv::Mat)
  │
  ▼ (QueuedConnection → UI 线程)
Widget::doShowVideo
  │  QImage + QPixmap → video_label_2->setPixmap()
  │
  ▼
屏幕显示
```

### 音频链路：MP4 → 扬声器

```
MP4 文件 (Mp4Demuxer 解封装得到音频 packet)
  │
  │ emit audioStreamStarted(codecId, sampleRate, channels, extradata)
  │ emit audioPacketReady(QByteArray 压缩数据, pts)
  ▼ (QueuedConnection → audiomanagerThread)
AudioManager::playEncodedAudioPacket (audiomanagerThread)
  │  avcodec_send_packet (FFmpeg 解码, 如 AAC → PCM)
  │  avcodec_receive_frame → decoded samples (如 fltp)
  │  swr_convert → S16 interleaved PCM
  │  sendPcmToAo → RK_MPI_AO_SendFrame(chn=0) 送入硬件音频输出
  │
  ▼
扬声器播放
```

### 播放同步机制

- `Mp4Demuxer` 的解封装线程作为 packet 分发时钟
- 第一个 packet 的 PTS 记为 `firstMediaUs`，墙钟记为 `playStartUs`
- 后续 packet 目标发送时间：`playStartUs + (pktMediaUs - firstMediaUs)`
- 解封装线程 sleep 到目标时间再发包，避免瞬间塞满下游缓冲
- 音视频 packet 按同一个 MP4 时间线离开 demuxer

## 编码约定

- 改动尽量小而集中，硬件依赖性强，宽泛重构风险高
- RK media 对象生命周期：enable → enable channel → enable resample；关闭时反向 disable 并清 buffer
- Qt worker slot 中避免无限等待
- H.264/MP4 转换用 FFmpeg bitstream filter 或已有 helper，不要手写字节替换
- MP4 packet 是压缩数据，AO 需要 PCM，中间必须解码 + `swr_convert`

## 调试

| 现象 | 排查方向 |
|------|----------|
| 视频有音频没有 | `audioStreamStarted` 是否发出；`startPlayback` 是否被阻塞 slot 卡住 |
| 第一次有声第二次无声 | AO stop/deinit、`RK_MPI_AO_WaitEos`、queued signal 顺序 |
| 播放卡顿 | 保留 PTS/DTS 发包节流，不要一次性推完 |
| 音画固定偏移 | 加可配置 offset，不要恢复固定 sleep |
| 音画逐渐漂移 | 检查 time_base、PTS 转换、AO/UI 缓冲 |
| 新录 MP4 没音频 | 检查 AI capture → `capFinishedToAudio` → `writeAudioToMp4` |
| MP4 播不了 | 确认 codec 是 H.264，当前 VDEC 只支持 H.264 MP4 |

## 已知限制

- 播放有时间戳节流但不是完整 master clock 播放器
- AO 缓冲可能引入固定延迟，需硬件实测后 offset 补偿
- UI 通过 Qt signal 传 `cv::Mat`，UI 忙时影响流畅度
- `.bin`/裸 H.264 fallback 路径仍保留，正常播放应使用 `.mp4`
