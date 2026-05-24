# cam-ui-widget 开发计划

## 一、应用层重构

### 1.1 目标

当前 `widget.cpp` 承担了太多职责：线程创建、信号槽连接、UI 逻辑、相册管理。重构为 Manager 架构，每个 Manager 封装自己的线程和内部连接，Widget 只做 UI 和组装。

### 1.2 架构划分

```
Widget (UI)
  │
  ├── CameraManager        摄像头采集 + 预览 + 拍照
  ├── KeyManager           按键输入轮询
  ├── ImuManager           IMU 数据采集与录制
  ├── AudioManager         音频采集(AI) + 播放(AO) + FFmpeg音频编解码
  └── VideoManager         视频编码(VENC) + 解码(VDEC) + MP4封装/解封装
```

### 1.3 Manager 职责详情

**CameraManager**
- 持有：`CameraManager` + `SavePicture`
- 内部连接：`cameracap::photoFinishedToSave` → `savepicture::do_save_picture`
- 对外信号：`previewFrame(cv::Mat)`、`photoSaved(QString)`、`videoFrameReady(cv::Mat, FILE*, uint64_t)`
- 对外槽：`startCapture()`、`stopCapture()`、`takePhoto()`、`startVideo()`、`stopVideo()`

**VideoManager**
- 持有：`VideoEncoder` + `Mp4Muxer` + `Mp4Demuxer` + `VideoDecoder`
- 内部连接：
  - `sigVideo2Mp4Mux` → `writeVideoToMp4`（录像视频写MP4）
  - `capFinishedToAudio` → `writeAudioToMp4`（录像音频写MP4，通过 AudioManager 提供接口）
  - 播放音频三件套（`audioStreamStarted/PacketReady/Finished`）→ AudioManager
- 对外信号：`videoFrameDecoded(cv::Mat)`、`recordStarted()`、`recordStopped()`
- 对外槽：`startRecord(QString path)`、`stopRecord()`、`play(QString path)`、`stopPlay()`

**AudioManager**
- 保持现有结构，增加对 VideoManager 的接口
- 提供：`writeAudioToMp4(QByteArray, uint64_t)` 给 VideoManager 调用（录像音频流）
- 提供：`startPlayback(codecId, sampleRate, channels, extradata)` 给 VideoManager 调用（播放音频流）

**KeyManager**
- 持有：`KeyControl`
- 对外信号：`photoPressed()`、`recordStartPressed()`、`recordStopPressed()`
- Widget 和各 Manager 只连 KeyManager 的信号，不直接连 KeyControl

**ImuManager**
- 持有：`IMUReader`
- 内部连接：`keycontrol::keyToStopvideo` → `imureader::stop_imu_record_txt`
- 对外槽：`startRecord(QString)`、`stopRecord()`

### 1.4 Widget 瘦身

重构后 Widget 只保留：
- UI 初始化和样式设置
- 相册/视频列表管理（增删改查）
- 页面切换逻辑
- Manager 信号 → UI 更新的连接（如 `previewFrame` → `doShow`）

### 1.5 实施顺序

1. 先拆 `CameraManager`（最独立，改动最小）
2. 再拆 `VideoManager`（涉及最多类，需要仔细设计接口）
3. 然后拆 `KeyManager`、`ImuManager`（简单）
4. 最后 `AudioManager` 接口调整

---

## 二、内核层优化：视频零拷贝链路

### 2.1 现状问题

当前录像链路存在多次不必要的内存拷贝：

```
摄像头 → [VI DMA buffer] → memcpy → [RGA 转换] → memcpy → [cv::Mat BGR]
         → memcpy → [VENC DMA buffer] → 硬件编码 → memcpy → [码流buffer]
         → MP4 封装
```

每一帧至少 3-4 次 `memcpy`，1080p BGR 帧一次就是 6MB，CPU 占用高且延迟大。

### 2.2 目标链路

```
摄像头 → [VI DMA-BUF] ─────────────────────────────┐
         │                                          │
         ├── RGA(imresize) → [预览 DMA-BUF] → UI 显示 │
         │                                          │
         └── RGA(imcvtcolor BGR) → [编码 DMA-BUF] → VENC ──→ 码流 → MP4
                                                                  ↑
                                                          零拷贝（DMA-BUF 共享）
```

关键点：VI、RGA、VENC 全部在内核态通过 DMA-BUF 共享 buffer，数据不经过用户态拷贝。

### 2.3 实现路径

**阶段一：理解现有 MPI 的 buffer 管理**

- 追踪 `RK_MPI_VI_GetChnFrame` 返回的 `pMbBlk`，确认底层是 DMA-BUF
- 追踪 `RK_MPI_VENC_SendFrame` 的 input buffer，确认是否可以接受外部 DMA-BUF
- 阅读 `rockit` 库源码（`librockit`），理解 `MB_BLK` 到 `dma_buf` 的映射关系
- 工具：`/sys/kernel/debug/dma_buf/bufinfo` 查看当前所有 DMA-BUF 的引用计数和大小

**阶段二：RGA 零拷贝转换**

- 当前 RGA 用 `wrapbuffer_virtualaddr`（用户态虚拟地址），改为 `wrapbuffer_fd`（DMA-BUF fd）
- VI 输出的 DMA-BUF fd 直接传给 RGA，RGA 输出也用 DMA-BUF fd
- 这样 RGA 转换完全在内核态完成，不需要用户态参与数据搬运

```cpp
// 当前：用户态地址 → RGA（需要数据已在用户态可访问）
wrapbuffer_virtualaddr(vi_frame_ptr, w, h, RK_FORMAT_YCrCb_420_SP);

// 优化：DMA-BUF fd → RGA（内核态直接操作，零拷贝）
int vi_dmabuf_fd = RK_MPI_MB_Handle2Fd(vi_frame.pMbBlk);
wrapbuffer_fd(dst_fd, w, h, RK_FORMAT_BGR_888);
```

**阶段三：VENC 零拷贝编码**

- RGA 输出的 DMA-BUF fd 直接传给 `RK_MPI_VENC_SendFrame`
- 避免 RGA → 用户态 memcpy → VENC 的路径
- 需要确认 `RK_MPI_VENC_SendFrame` 是否支持外部 DMA-BUF（通过 `MB_EXT_CONFIG_S` 或 fd 方式）

**阶段四：码流零拷贝传递**

- VENC 输出的码流 buffer 也通过 DMA-BUF fd 传递给 Mp4Muxer
- `av_read_frame` / `av_interleaved_write_frame` 的 buffer 可以用 `av_buffer_create` 包装 DMA-BUF fd
- 避免码流数据从硬件 buffer memcpy 到用户态再拷给 FFmpeg

### 2.4 性能指标

| 指标 | 当前（估算） | 目标 |
|------|-------------|------|
| 每帧 memcpy 次数 | 3-4 次 | 0 次（纯内核态流转） |
| 1080p BGR 单帧拷贝量 | ~18 MB | 0 MB |
| 录像 CPU 占用 | 高（大量 memcpy） | 降低 50%+ |
| 端到端延迟（按键→首帧写入） | 待测 | 降低 30%+ |

### 2.5 验证方法

- `perf top` 对比优化前后 `memcpy` 热点占比
- `/sys/kernel/debug/dma_buf/bufinfo` 确认 buffer 引用计数正确（无泄漏）
- `ftrace` 追踪 `dma_buf_map_attachment` / `dma_buf_unmap_attachment` 调用链
- 实际录像对比 CPU 占用和帧率

---

## 三、后续可探索方向

| 方向 | 内容 | 优先级 |
|------|------|--------|
| V4L2 直采 | 绕过 MPI，直接用 V4L2 API 从 `/dev/video0` 拍帧 | 中 |
| DRM/MIPI OSD | 用 libdrm 做硬件图层叠加，运动参数直接渲染到视频上 | 中 |
| IIO IMU 驱动 | 把 IMU 从字符设备改为标准 IIO 子系统接口 | 低 |
| PREEMPT_RT | 打 RT 补丁，测量并优化实时调度延迟 | 低 |
| 功耗优化 | CPU DVFS + IP 时钟动态开关 | 低 |
