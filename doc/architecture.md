# 系统架构与数据流

> 历史文档：旧应用源码已在重构分支移除。以下链路和路径仅供追溯，新架构见 [refactoring.md](refactoring.md)，当前构建状态见 [README](../README.md)。

> 本文记录 cam-ui-widget 的录像/播放链路、零拷贝实现、音视频同步机制。
> 文件职责与构建运行见根目录 [`../CLAUDE.md`](../CLAUDE.md)；调试方法见 [`debugging.md`](debugging.md)；问题排查见 [`troubleshooting.md`](troubleshooting.md)。

`Widget` 创建 worker 对象并移动到独立 QThread（`CameraManager`/`SavePicture`/`VideoEncoder`/`KeyControl`/`Mp4Demuxer`/`VideoDecoder`/`Mp4Muxer`/`AudioManager`/`IMUReader`）。

**踩坑提醒**：worker slot 中不能无界阻塞，否则 queued signal 全部排队无法执行。这是“第二次播放没声音”类 bug 的常见根因。

---

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

### 视频链路：摄像头 → MP4（零拷贝）

```
VI 硬件 (RK_FMT_YUV420SP, 1920×1080, DMA-BUF)
  │
  ▼
CameraManager::startCapture (cameramanagerThread)
  │
  ├── 预览路径（不变）：
  │   wrapbuffer_virtualaddr(dat) → RGA imcvtcolor → RGB888 552×368 → cv::Mat
  │   └── capFinishedToUI ──▶ Widget::doShow
  │
  └── 录像路径（零拷贝）：
      RK_MPI_MB_Handle2Fd(vi_frame) → src_fd
      wrapbuffer_fd(src_fd) → RGA imcvtcolor → BGR888 1920×1080
      └── 写入 bgr_fd_[idx]（DMA 双缓冲，无 CPU 拷贝）
          │
          │ capFinishedToVideo(void* MB_BLK, PTS)  ── 跨线程信号 ──▶
          ▼
VideoEncoder::do_save_video (videoencoderThread)
  │  直接使用传入的 MB_BLK（零拷贝，无 memcpy）
  │  RK_MPI_VENC_SendFrame (BGR888 → H.264 硬件编码)
  │  RK_MPI_VENC_GetStream (得到 H.264 Annex-B 码流)
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

#### 零拷贝实现要点

录像路径全程 DMA-BUF fd 操作，CPU 不碰像素数据：

- **RGA 源**：`RK_MPI_MB_Handle2Fd` 提取 VI 帧的 DMA-BUF fd，用 `wrapbuffer_fd` 传给 RGA，避免 CPU 虚拟地址映射
- **RGA 目标**：CameraManager 构造时分配 2 个 BGR DMA buffer（`RK_MPI_MMZ_Alloc` + `RK_MPI_MMZ_Handle2Fd`），RGA 直接写入，无 cv::Mat 堆分配
- **双缓冲**：帧 N 写 `bgr_blk_[0]`，帧 N+1 写 `bgr_blk_[1]`，交替使用，避免 RGA 覆盖 VENC 还未读完的 buffer
- **MB_BLK 传递**：信号 `capFinishedToVideo(void*, uint64_t)` 直接传 MB_BLK 指针，VideoEncoder 用 `reinterpret_cast<MB_BLK>` 还原后直接送 VENC
- **无 memcpy**：整个录像路径 0 次 CPU 拷贝，VI → RGA → VENC 全部硬件 DMA 操作


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

**启动**：`KeyControl` 触发 → `CameraManager::startVideo` → 第一帧到达时创建 `.mp4`/`.jpg` → `sigVideoMp4Init` 同时触发 `AudioManager::startCapture` + `Mp4Muxer::initMp4Mux`

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

---

## 预览零拷贝方案（讨论）

> 当前预览路径仍有 CPU 拷贝与开销，录像路径已零拷贝但预览未跟进。本节记录优化方案与约束分析。

### 现状瓶颈

预览路径（`cameramanager.cpp:41-94` → `widget.cpp:337-407`）：

```
VI(DMA-BUF, NV12 1920×1080)
  → RK_MPI_MB_Handle2VirAddr → 用户态虚拟地址 dat
  → wrapbuffer_virtualaddr(dat)            ── RGA src（用户态内存）
  → wrapbuffer_virtualaddr(ui_frame.data)  ── RGA dst（用户态内存）
  → imresize + imcvtcolor → RGB888 552×368
  → cv::Mat ui_frame（每帧堆分配 ~600KB）
  → emit capFinishedToUI(cv::Mat)          ── 跨线程信号（浅拷贝，无像素拷贝）
  → Widget::doShow
      → cv::putText 画 OSD(FPS/时间/录像时长)  ── CPU 写像素
      → QImage 包裹 img.data（浅拷贝）
      → QPixmap::fromImage                  ── 格式转换
      → QLabel::setPixmap                   ── 软件渲染到 backing store → /dev/fb0
```

CPU 开销点：

1. **每帧 `cv::Mat` 堆分配/释放**（~600KB）
2. **RGA 用 `virtualaddr` 模式**：源/目标都走用户态内存，有 map/unmap 开销，与录像路径的 `wrapbuffer_fd` 不一致
3. **`cv::putText` 画 OSD**：UI 线程写像素
4. **`QPixmap::fromImage` + QLabel 软件渲染**：转换 + blit 到 fb（CPU）

### 环境约束：linuxfb

`export QT_QPA_PLATFORM=linuxfb`，Qt 走 `/dev/fb0` 纯软件渲染：

- 无 GPU 介入（eglfs 才有 Mali GPU 加速）
- 无合成器，单一 backing store blit 所有内容，无 plane 图层叠加
- VO/DRM 直接显示会和 fb0 抢显示控制器，能否共存需实测

**结论**：linuxfb 下真·零拷贝到屏幕做不到（最后一步 Qt blit 到 fb 必走 CPU）。能做的是“数据侧零拷贝”——采集/转换链路消除 CPU 拷贝，显示仍由 Qt 软件渲染。

### 方案分层

**方案 A：数据侧零拷贝（推荐先行，低风险）**

仿照录像路径：CameraManager 预分配 2~3 个 RGB888 DMA-BUF（`RK_MPI_MMZ_Alloc` + `RK_MPI_MMZ_Handle2Fd`），RGA 源/目标都用 `wrapbuffer_fd`，输出 DMA-BUF 后 mmap 给 `QImage`（浅包裹）再 `setPixmap`。

```
VI(DMA-BUF fd) → wrapbuffer_fd(src_fd) → RGA → 预分配 DMA-BUF fd(dst) → mmap → QImage → setPixmap
```

- 收益：去掉 Mat 分配 + virtualaddr 开销（主要大头）
- 保留：Qt 软件 blit（552×368 ~800KB/帧，开销可接受）+ putText 写 mmap（开销可忽略）
- 改动范围：`cameramanager.cpp` 预览分支 + `Widget::doShow` 接收方式
- 风险：低，与录像零拷贝路径同一套机制

**方案 B：rockit VO 直接显示（真零拷贝，RK-native）**

rockit VO 把 DMA-BUF 直送 VOP 显示，支持视频层 + OSD 图层叠加，CPU 0 拷贝。

- 难点：与 linuxfb 的 fb0 协调（显示控制器占用）、Qt 窗口区域让出给 VO、当前未用 VO
- 风险：架构级改动

**方案 C：DRM/KMS prime attach plane**

libdrm 把 RGA 输出 DMA-BUF fd attach 到 DRM video plane，OSD 用另一 plane 或 GPU 叠加。

- 难点：与 linuxfb 冲突、需自管 plane/层叠
- 灵活但工作量大

**方案 D：eglfs + GPU texture 导入（linuxfb 下不可用）**

切 Qt 平台插件到 eglfs（Mali GPU），`GL_OES_EGL_image_external` 导入 DMA-BUF 为 GL texture，`QOpenGLWidget` 采样显示，OSD 用 GPU 渲染。

- linuxfb 下不可用；且 QLabel → QOpenGLWidget 改动大
- 若未来切 eglfs 可作为彻底零拷贝方案

### 关键拦路虎：OSD

`cv::putText` 依赖 CPU 可写像素内存。只要 OSD 写进像素，数据就必须 CPU 可写，与“数据驻留 DMA-BUF/GPU 不可写”矛盾。彻底零拷贝需把 OSD 改为**图层叠加**（VO/DRM plane）或 **GPU 渲染**。

- 方案 A：可继续写 mmap 的 DMA-BUF（开销小，可接受）
- 方案 B/C/D：必须改 OSD 实现方式

### 建议路径

1. 先做**方案 A**，低成本拿到主要收益（Mat 分配 + virtualaddr 消除）
2. 实测 CPU 占用，若仍高再评估切 eglfs 或引入 VO

---

## 已知限制

- 播放有时间戳节流但不是完整 master clock 播放器
- AO 缓冲可能引入固定延迟，需硬件实测后 offset 补偿
- UI 通过 Qt signal 传 `cv::Mat`，UI 忙时影响流畅度
- 正常播放应使用 `.mp4`
- 当前 VDEC 只支持 H.264 MP4
