# 零拷贝链路开发计划

> 目标：把**预览**和**播放**两条路径从「CPU 软件光栅 + eglfs 整页 texture 上传」推进到「DMA-BUF → EGLImage → GL external texture → scanout」的真零拷贝。
> 录像链路已零拷贝（见 [architecture.md](architecture.md) 录像一节），不在本计划范围。
> 本文不动代码，只规划方案、阶段、依赖、风险、验收。
> 现状背景（必看）：[`architecture.md`](architecture.md) 「预览零拷贝方案（讨论）」一节，「环境约束：linuxfb」一段已过时——eglfs 现已打通。

### 技术路线（不可妥协）

**唯一路径：DMA-BUF + OpenGL**。VI/VDEC 输出的 DMA-BUF fd 通过 `eglCreateImageKHR(EGL_LINUX_DMA_BUF_EXT)` 包成 EGLImage，绑到 `GL_TEXTURE_EXTERNAL_OES`，shader 采样上屏；OSD 在同一 GL context 里由 `QPainter(this)` GPU 合成。CPU 全程不接触像素。

不走任何"RGA→BGR888 mmap→QImage"的中间方案——那条末段是 CPU blit，不是真零拷贝。本计划不允许出现 CPU 像素搬运。

---

## 0. 背景与定位

### 0.1 当前进度（2026-07）

- Qt 平台插件已由 `linuxfb` 切到 `eglfs` + `eglfs_kms`，GPU 负责最终一公里（backing store → GL texture → DRM scanout）。
- eglfs 只是把"整页 texture 上屏"搬到了 GPU，**界面绘制段仍在 CPU**：预览和播放还是要走 `cv::Mat → QPainter 软件光栅`。
- 这就给真正的零拷贝铺平了路：现有 eglfs 上下文里就可以做 `DMA-BUF → EGLImage → GL texture` 导入（`architecture.md:278` 方案 D 现已可走）。

### 0.2 三条路径现状速览

| 路径 | 收尾段（CPU 还是 GPU） | DMA-BUF 是否落屏 | 主要 CPU 开销 |
|---|---|---|---|
| 预览 | CPU 绘制 + GPU 上屏 | 否 | `cv::Mat` 堆分配 + `virtualaddr` RGA + `putText` OSD + `QPixmap::fromImage` |
| 播放 | CPU 绘制 + GPU 上屏 | 否 | 同上，VDEC 输出已是 DMA-BUF 但被映射到用户态 |
| 录像 | 全硬件 | 录像路径不显示 | 0（已无 CPU 拷贝）|

### 0.3 目标态

| 路径 | 收尾段 | 落屏方式 | CPU 像素搬运 |
|---|---|---|---|
| 预览 | GPU | GL_OES_EGL_image_external 采样 DMA-BUF | 0 |
| 播放 | GPU | 同上，VDEC DMA-BUF 直导 GPU | 0 |
| OSD（录像时长/时间） | GPU | QOpenGLWidget 内 `glRasterPos` / 文字贴图 | 仅少量 |

---

## 1. 关键技术决策

### 1.1 显示控件的选择

`QLabel::setPixmap` 是软件光栅路径，必须替换。三个候选：

| 方案 | 描述 | 评估 |
|---|---|---|
| `QOpenGLWidget` 子类 | 自己在 `paintGL` 里 EGLImage + shader 采样 | **唯一采用**。控件替换 `video_label`/`video_label_2`，UI 其余部分不动 |
| `QOpenGLWidget` + GPU 画 OSD | 同上，OSD 用 `QPainter` 在 `QOpenGLWidget::paintGL` 里画（GL paint device = GPU 光栅化） | 配套必选 |
| QQuickWidget + Qt Quick | 接入 Scene Graph | 不采用。引入 Quick，改动量大，与现有纯 Widgets 体系混用 |

**统一选择**：自定义 `VideoGLWidget : public QOpenGLWidget`，统一承载预览、录像预览、播放三个 label 的职责；OSD 在其 `paintGL` 内用 GPU 完成。

### 1.2 DMA-BUF 导入路径

唯一采用：

```
eglCreateImageKHR(dpy, ctx, EGL_LINUX_DMA_BUF_EXT, fd, attr_list)
  → EGLImage
glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, image)
  → 外部纹理
shader 中 vec4 sample = texture2D(samplerExternalOES, uv)
```

像素格式：VI/VDEC 输出的 NV12/UV21 原样喂 OES external sampler，**不经过 RGA**。如果实测个别 stride/格式被驱动拒，再按需调整 attr_list（offset/pitch），但仍在 GPU 侧解决；**绝不在 CPU 侧做像素搬运**。

> 详见 [`architecture.md`](architecture.md) 预览一节中的方案 D，本文档把它落地。

### 1.3 PoC 验证准则（首要前提）

字符串查询 `EGL_EXT_image_dma_buf_import` 在扩展名表里**不能作为否决依据**——Mali wayland 变体常见只导出函数指针、不声明扩展名的情况。判定**必须用真调用**：

1. 用 `/dev/dma_heap/system` 分配一块 NV12 dma_buf（或直接拿 VI 帧的 fd）
2. `eglCreateImageKHR(EGL_LINUX_DMA_BUF_EXT, ...)` 真实调用，检查 `eglGetError()` 是否 `EGL_SUCCESS`
3. `glEGLImageTargetTexture2DOES` 绑定，glClear 看屏幕出图/颜色变化
4. 若出现 `EGL_BAD_PARAMETER`、`EGL_BAD_MATCH` 等再判定具体 target/格式不支持

`zcpoi.cpp` 第一版只做了字符串查询，结论无效；M1 之前必须补 PoC2 真调验证。**M1 不得在没有真调验证的情况下推进**。

---

## 2. 阶段规划

整体三阶段，逐阶段独立可发布、可验收、可回滚。

### 阶段一：预览零拷贝（VI → GPU → 屏）

#### 目标
- 消除每帧 `cv::Mat` 堆分配（约 600KB/帧）
- 消除 `wrapbuffer_virtualaddr` 用户态映射
- 消除 `QPixmap::fromImage` 的转换 blit
- 消除 CPU 写 `fb0` 的整页 blit（eglfs 本来已经不用 fb0，但 backing store 软件 blit 还在）
- OSD 移到 GPU

#### 数据流（目标态）

```
VI 硬件 (NV12 1080p, DMA-BUF fd)
   │
   ▼  (零 CPU)
CameraManager::startCapture
   │  RK_MPI_MB_Handle2Fd(vi_frame.pMbBlk) → src_fd
   │  emit capFinishedToUI(int fd, int w, int h, uint64_t pts)   ← 信号由 cv::Mat 改为 fd
   ▼
Widget::onPreviewFrame(fd, w, h, pts)    ← UI 线程
   │  m_glPreviewWidget->setDmaBuf(fd, w, h)   ← 仅登记
   ▼
VideoGLWidget::paintGL (UI 线程 / GL context)
   │  eglCreateImageKHR(fd) → m_image
   │  glEGLImageTargetTexture2DOES(m_image) → TEXTURE_EXTERNAL_OES
   │  shader 采样 → 全屏贴图
   │  QPainter(this) 画 OSD 文字（GPU composited）
   ▼
DRM scanout（Mali → VOP）
```

#### 改动点（仅列位置与意图，不写实现）

| 文件 | 改动 |
|---|---|
| `cameramanager.h/cpp` | 预览分支输出由 `cv::Mat` 改为 DMA-BUF fd + 几何；保持录像分支不变（已是 NV12 MB_BLK）|
| `main.cpp` | 无 |
| `widget.h/cpp` | 新增 `VideoGLWidget`；`video_label`/`video_label_3` 由 `QLabel` 改为 `VideoGLWidget`；`doShow` 接 fd 转发；OSD 文字绘制移到 `VideoGLWidget::paintGL` 用 `QPainter(this)` |
| `widget.ui` | 把对应 QLabel 提升为 `VideoGLWidget`（Qt Designer 中 Promote）|
| `.pro` | `QT += widgets opengl`，必要时链 `EGL`/`GLESv2` |

#### VI 帧生命周期

当前录像用 `pendingViFrames_` + 回调释放（`cameramanager.cpp:87-91`）。预览走 fd 后：
- VI 帧在 GPU 持有 EGLImage 期间不能 `ReleaseChnFrame`
- 方案：UI 收到 fd 后 `makeCurrent` 立即 `eglCreateImageKHR`，引用一份，立即发信号回 CameraManager 释放原 VI 帧；EGLImage 在下一帧时销毁重建
- 或：让 VideoGLWidget 维护一个 2~3 张的 ring，CameraManager 等回执再释放（与录像路径同构，可复用 `pendingViFrames_` 机制）

**选后者**，与录像既有机制同构，避免 EGLImage 长期持有 DMA-BUF 造成的 display pipeline 抖动。

#### 风险

| 风险 | 应对 |
|---|---|
| Mali 真调 `eglCreateImageKHR(EGL_LINUX_DMA_BUF_EXT)` 返回 `EGL_BAD_*` | 不靠扩展名判断；按 PoC2 实测错误码逐项调 attr_list（pitch/offset/fourcc），必要时讯问 BSP；**不允许退回 CPU 路径**，限制在 GPU 侧解决 |
| Mali 与 VOP 处理 YUV sampler 时跨越 DRM 计划支持需 sync | 实测 vsync；必要时用 `EGL_ANDROID_native_fence_sync` |
| Qt `QOpenGLWidget` 在 eglfs 下的隐式 FBO 与外部 texture 句柄冲突 | 自管 FBO 不要与 Qt 自动 backing FBO 混 |
| 与原 UI 切页（stackedWidget）切换时 GL context 丢失 | 走 `QOpenGLWidget::initializeGL` 重建；信号触发 `update()` |
|eglfs_kms 单窗口策略下多个 VideoGLWidget 共享 context | `QSurfaceFormat::setShareContext` 或 `AA_ShareOpenGLContexts`（见 `main.cpp` 当前未设）|
| VI 输出 stride 与 width 不等（NV12 stride 对齐） | EGLImage attr_list 传 `EGL_DMA_BUF_PLANE0_PITCH_EXT` 正确指定；shader 不参与 |

#### 验收

- `top -H` 主线程 CPU 较 M0 基线下降 ≥20%
- `perf top` 主线程不再出现 `memcpy` / `_arm_memset` 热点
- `/sys/kernel/debug/dma_buf/bufinfo` 仅见 VI、EGLImage、backing-store texture 共 ~3 类 buffer，无每帧新增
- 视觉无撕裂、无绿边、OSD 文字清晰可见、切换界面不闪黑

### 阶段二：播放零拷贝（VDEC → GPU → 屏）

#### 目标
- VDEC 输出 DMA-BUF 直送 GPU，不再走 `wrapbuffer_virtualaddr` + `cv::Mat` + `QPixmap`
- OSD 锦标暂时无需（播放页 `video_label_2` 现亦无 OSD），留 paintGL 接口

#### 数据流（目标态）

```
MP4 文件
   ▼
Mp4Demuxer (mp4demuxerThread)
   ▼  AVCC→Annex-B，送 VDEC
VDEC 硬件解码 → DMA-BUF fd (NV21)
   ▼
VideoDecoder::do_get_frame
   │  RK_MPI_MB_Handle2Fd → dec_fd
   │  emit VideodecToUI(int dec_fd, int w, int h, uint64_t pts)  ← 由 cv::Mat 改为 fd
   ▼
Widget::onPlayFrame(dec_fd, w, h, pts)
   │  m_glPlayWidget->setDmaBuf(dec_fd, w, h)
   ▼
VideoGLWidget::paintGL → 同阶段一机制 → scanout
```

#### 改动点

| 文件 | 改动 |
|---|---|
| `videodecoder.h/cpp` | `do_get_frame` 输出由 `cv::Mat` 改为 fd；保持现有 VDEC 帧对齐（16字节对齐）|
| `widget.h/cpp` | `doShowVideo` 改为接 fd 并转发 `VideoGLWidget`；`video_label_2` 提升为 `VideoGLWidget` |
| `widget.ui` | `video_label_2` 提升 |
| `mp4demuxer.cpp` | 不动；现状已按时钟节流 |

#### 与阶段一共享

- `VideoGLWidget`、EGLImage 导入代码完全复用
- 帧生命周期 ring 机制复用
- 回滚路径同

#### 特殊点

- VDEC 停止时 `FRAME_FLAG_SNAP_END`（`videodecoder.cpp:58`）的处理保持不变
- `Mp4Demuxer` 节流不变（墙钟）
- 切回预览页（`switchToPage0`）必须确保 VDEC GL 资源销毁，避免占用 plane

#### 风险

| 风险 | 应对 |
|---|---|
| VDEC 输出格式 NV21 vs NV12 vs RGB565 | `RK_MPI_VDEC_QueryFFMpegStatus` 取格式；shader/FMT 自适应 |
| VDEC 输出 stride 与 width 不等 | shader 用 `width`/`stride` 区分；或先 RGA 归一 stride |
| FAST/普通模式 VDEC 内部 buffer 池大小 | 帧 ring 深度需对齐，否则 VDEC 自己排队会丢帧 |
| 切 page 时 GL 资源抢 duration | paintGL 中 lazy init，stop 后 explicit destroy |

#### 验收
- 播放 1080p MP4 主线程 CPU 较现状 `-30%`，无掉帧
- 倒退 seek 后首帧能在 2 个 vsync 内显示
- 不出现残留帧（GL 资源生命周期正确）

---

## 3. 公共子模块：VideoGLWidget 设计草案

仅描述接口，不写实现。

### 职责

- 接受 DMA-BUF fd + 宽高 + 格式标记
- `QOpenGLWidget::initializeGL`：编译 shader，建 sampler，取 extension 入口
- `QOpenGLWidget::paintGL`：销毁上一帧 EGLImage、创建本帧 EGLImage、绑定 TEXTURE_EXTERNAL_OES、画全屏 quad；OSD 由上层 `setOverlay(text/style)` 触发 QPainter(this) 绘制
- `resizeGL`：重算 viewport、矩阵
- `~VideoGLWidget`：makeCurrent → 销毁所有 GL 资源 → doneCurrent

### 接口（示意）

```
class VideoGLWidget : public QOpenGLWidget {
public:
    void setDmaBuf(int fd, int w, int h, quint32 fourcc = DRM_FORMAT_NV12);
    void setOverlay(const QString& text, QPoint pos, QColor color, int pointSize);
    void clear();   // 切 page 时清屏
protected:
    void initializeGL() override;
    void resizeGL(int w,int h) override;
    void paintGL() override;
private:
    // EGLImage handle
    // program / sampler / vbo
    // 当前帧 fd 等
    // overlay 文本与样式
};
```

### 共享 context

eglfs 单窗口下，应用可设 `QApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true)`（`main.cpp` 现状未设），让预览/播放/未来录像预览三个 `VideoGLWidget` 共享一份 shader 资源。

---

## 4. 测量与基线

### 4.1 基线采集（动代码前必做）

按 [`refactoring.md`](refactoring.md) 二节相同的格式，跑当前 eglfs 版本采集：

| 状态 | VIRT | RES | %CPU | %MEM | 备注 |
|---|---|---|---|---|---|
| 仅预览（eglfs） | 1470.8 MB | 98.4 MB | 109.9% | 5.0% | 2026-07-04 实测，单次 top 采样 |
| 录像（eglfs） | ? | ? | ? | ? | 待采集 |
| 播放（eglfs） | ? | ? | ? | ? | 待采集 |

**与 linuxfb 基线对比**（数据自 [`refactoring.md`](refactoring.md) §二）：

| 状态 | 平台 | VIRT | RES | %CPU | %MEM |
|---|---|---|---|---|---|
| 仅预览 | linuxfb（2026-05-24） | 1360 MB | 100 MB | 74.5% | 5.1% |
| 仅预览 | **eglfs（2026-07-04）** | 1471 MB | 98.4 MB | **109.9%** | 5.0% |

观察：
- RES / %MEM 基本持平 → eglfs 仅替换上屏路径，内存侧无显著变化（符合预期）
- **%CPU 反而升 ~35%** → 推测 eglfs 每帧把 backing store 软件光栅完后整页 texture upload 到 Mali 的开销，超过了原 linuxfb 直接 memcpy 写 fb0 的开销
- VIRT 略增 ~110MB → Mali/EGL 栈映射进来

这条数据印证 M0→M1 的必要性：eglfs 让 GPU 上了屏，但 `QPainter` 软件光栅 + 整页 texture upload 仍在 CPU 上且更贵。必须把摄像头帧直接做成 GL texture（M1/M2）才能压住这个 ~110%。

测量手段（见聊天 [`tools` given earlier]）：
```
top -b -d 1 -n 30 | grep ai-cam
cat /proc/$(pidof ai-cam-ui-widget)/status | grep VmRSS
top -H -p $(pidof ai-cam-ui-widget) -b -n1
cat /sys/kernel/debug/mali/utilisation 2>/dev/null   # GPU 利用率（路径按 BSP 调整）
cat /sys/kernel/debug/dma_buf/bufinfo 2>/dev/null     # DMA-BUF 引用计数
perf top -p $(pidof ai-cam-ui-widget)                # memcpy/QImage 热点
```

### 4.2 各阶段达标门槛

| 阶段 | 主线程 %CPU | RES | 期望变化 |
|---|---|---|---|
| 基线 (eglfs 软件光栅) | X | .. | — |
| 阶段一完成 | X × 0.7~0.8 | 持平或略升（OSD 纹理） | `perf top` 不再见到 `arm_mem*` 大热 |
| 阶段二完成 | 较播放基线 -30% | 持平 | DMA-BUF only |

---

## 5. 实施顺序与里程碑

时间不分具体日期，按依赖条款排序：

1. **M0 基线采集**（已完成预览态；录像/播放态待补，不动代码，0.5 天）
2. **M0.5 PoC2 真调验证**（前置必做，不可跳）：`/dev/dma_heap/system` 分配 NV12 dma_buf → `eglCreateImageKHR` 真调 → `glEGLImageTargetTexture2DOES` 绑定 → 屏幕出图。验证未通过不进 M1（1 天）
3. **M1 公共部件 `VideoGLWidget` 雏形**（基于 PoC2 已验证的 EGLImage 导入路径，封装 `setDmaBuf(fd,w,h,fourcc)` + shader + 全屏 quad，2 天）
4. **M2 预览接 fd**（替换 `capFinishedToUI` 信号、改造 CameraManager 预览分支，2 天）
5. **M3 预览 OSD 重新落地**（`putText` → `QPainter(this)` GPU 合成，1 天）
6. **M4 预览验证 + 验收**（验收清单，1 天）
7. **M5 播放接 fd**（替换 `VideodecToUI`、改造 VideoDecoder，2 天）
8. **M6 播放验证**（验收清单，1 天）

每个 M 完成都能独立 commit、独立上线。**不允许在任何阶段以 CPU mmap 作为后备显示路径**——受阻时定位驱动/attr 问题在 GPU 侧解决。

---

## 6. 风险总览

| 类别 | 条目 | 阶段 | 等级 | 备注 |
|---|---|---|---|---|
| 平台 | Mali 不暴露 `EGL_EXT_image_dma_buf_import` 扩展名 | 一/二 | 中 | 字符串查询无效；以 PoC2 真调 `eglCreateImageKHR` 的返回值为准 |
| 平台 | Mali 真调 `eglCreateImageKHR(EGL_LINUX_DMA_BUF_EXT)` 返回错误 | 一/二 | 高 | 在 GPU 侧用 attr_list 调通；不允许退 CPU |
| 平台 | `GL_OES_EGL_image_external` 真不存在 | 一/二 | 中 | PoC2 探测结果是 YES（`zcpoi.cpp` 第一版已验）|
| 平台 | eglfs_kms 与 VIDEOPROC/colorkey 冲突 | 一 | 中 | 实测，必要时 `QT_QPA_EGLFS_KMS_CONFIG` 限制 eglfs 占 plane |
| 驱动 | linux_sdk buildroot 的 `libmali` 实际是 wayland 变体 | 一 | 中 | 真调验证后方知 dmabuf target 是否接受；不接受者换 GBM 变体 |
| Qt | `QOpenGLWidget` 在 eglfs 下多 widget 共享 context 异常 | 一 | 中 | `AA_ShareOpenGLContexts` |
| Qt | stackedWidget 切页导致 GL context 重建抖动 | 一/二 | 中 | `initializeGL` 重建 + 信号触发 `update()` |
| 媒体 | VI / VDEC stride ≠ width | 一/二 | 中 | EGLImage attr_list 传 pitch |
| 媒体 | VI 输出 NV12 / VDEC 输出 NV21 格式标志差异 | 一/二 | 低 | shader 不变，仅 attr_list fourcc 不同 |
| 工程 | UI 主线程阻塞在 `makeCurrent` | 一 | 低 | EGLImage 重建是异步同步对象，文件上传/绑定本身轻量 |
| 工程 | DMA-BUF 引用计数泄漏 | 一/二 | 中 | `/sys/kernel/debug/dma_buf/bufinfo` 验 |

### 6.1 PoC2 是 M1 的强制前置

`zcpoi.cpp`（第一版）只查扩展名，结论作废。M1 之前必须写 PoC2：

1. `open("/dev/dma_heap/system")` + `ioctl(DMA_HEAP_IOCTL_ALLOC)` 拿一块 dma_buf fd（或更省事直接在 `RGA` 输出一个 dma_buf）
2. `mmap` 写入一段已知灰度值（仅 PoC 用，目标侧任其显示验证）
3. `eglCreateImageKHR(dpy, EGL_NO_CONTEXT_KHR, EGL_LINUX_DMA_BUF_EXT, fd, attr_list)`
4. 检查 `eglGetError()`，若非 `EGL_SUCCESS` 调整 attr_list 直到成功
5. `glEGLImageTargetTexture2DOES` 绑外部纹理 → shader 采样全屏贴图
6. 校验屏幕上的颜色与第 2 步写入一致

**只有 PoC2 出图，M1 才能开工**。若 PoC2 在驱动层彻底调不通，按本计划原则：不退 CPU，而是评估换 `libmali` GBM 变体（重 build Qt/驱动），或继续在 GPU 侧调整 attr_list / fourcc / modifier 等参数直到驱动接受。

---

## 7. 与既有计划的关系

- [architecture.md](architecture.md) §「预览零拷贝方案（讨论）」：本计划是它的落地版，linuxfb 约束不再适用，方案 D（eglfs + GPU texture）为唯一路径
- [refactoring.md](refactoring.md) §「三、内核层优化：视频零拷贝链路」：录像是其完成品；预览播放零拷贝未列入，本文补齐
- [refactoring.md](refactoring.md) §「一、应用层重构」：Manager 架构与本文正交。建议先做本文 M1~M4 再合并重构，避免接口抖动期双重改动
- [refactoring.md](refactoring.md) §「四、后续可探索方向」：DRM/MIPI OSD 不在本计划范围

---

## 8. 备注 / 未决问题

- 用 V4L2 直采绕过 rockit MPI 的可行性见 [refactoring.md](refactoring.md) §「四」，**本计划不涉及**，按现状 VI/MPI 路径推进
- IMU / 音频 / 录像封装器均不动
- `keycontrol` 不动
- `main.cpp` 的旋转 QGraphicsView + fitInView 配置可能在新方案下跳过旋转：预览阶段直接在 shader 里有 `uRotation` 控制更简洁（免 rotation proxy），可选优化点，不强制
- eglfs_kms 的 plane 占用策略待后续实测
- `zcpoi.cpp` 第一版（字符串查询）遗留，作 PoC1 历史参考；M0.5 的 PoC2 是真调验证版本，**两份独立**
- `zcpoi.cpp` 已删除，PoC1 不再保留在仓库中

---

## 9. 调试记录（2026-07-04 实战日志）

> 本节记录预览零拷贝链路从 M0 到 M2 的完整调试过程，包括每个问题的现象、误判、定位、最终解决方式。
> 目的：下次恢复调试状态时，按此节快速定位到当前进度，避免重蹈弯路。

### 9.1 M0 基线采集

在 eglfs 软件光栅版本（QLabel + cv::Mat + QPainter）上采集预览态资源占用：

```
PID  USER  VIRT     RES    %CPU   %MEM
1160 root 1470.8m  98.4m 109.9   5.0   ./ai-cam-ui-widget
```

与 linuxfb 基线对比：RES/%MEM 持平，**CPU 反升 35%**（74.5% → 109.9%）。原因：eglfs 每帧把 backing store 软件光栅完后整页 texture upload 到 Mali，比 linuxfb 直接 memcpy 写 fb0 更贵。印证了零拷贝的必要性。

### 9.2 PoC1 字符串查询（已废弃）

写 `zcpoi.cpp` 第一版，用 `glGetString(GL_EXTENSIONS)` + `eglQueryString(EGL_EXTENSIONS)` 查扩展名。

**结果**：
- `GL_OES_EGL_image_external`：YES
- `GL_OES_EGL_image`：YES
- `EGL_EXT_image_dma_buf_import`：**no**（EGL 串里没查到）
- `eglCreateImageKHR` 函数指针：非空
- `glEGLImageTargetTexture2DOES` 函数指针：非空

**误判**：当时判断"Mali wayland 变体不暴露 dma_buf import 扩展，路径1 不可走"。**这个结论是错的**——字符串查询失败是因为 PoC1 用 `eglGetDisplay(EGL_DEFAULT_DISPLAY)` + `eglInitialize` 查的，但 `eglInitialize` 失败了（Qt 已 init 过），查的 display 不是 Qt 用的那个。

**教训**：扩展名字符串查询**不能作为否决依据**。Mali wayland 变体有函数指针但不一定在所有 display 上声明字符串。必须用真调用验证。`zcpoi.cpp` 已删除。

### 9.3 libmali 内部字符串确认

用 `strings /usr/lib64/libmali.so.1.9.0 | grep` 确认 libmali 内部**确实有** `EGL_EXT_image_dma_buf_import`、`EGL_EXT_image_dma_buf_import_modifiers`、`DRM_FORMAT_NV12`、`DRM_FORMAT_NV21`、`GL_EXT_YUV_target` 等字符串。

**结论**：libmali 实现了 dma_buf import 功能，只是 PoC1 查错了 display。

### 9.4 M1 VideoGLWidget 雏形

在 `widget.ui` page0 添加 `QOpenGLWidget` 名 `openGLWidget`（552×368），在 Qt Designer 里提升为 `VideoGLWidget`（局部包含 `videoglwidget.h`）。

`.pro` 加 `QT += opengl` + `LIBS += -lEGL -lGLESv2`。

`VideoGLWidget` 初版只 `glClear` 黑底，验证 GL context 在 eglfs 下能起来。编译通过，部署跑，黑屏不崩——M1 雏形通过。

### 9.5 M2 接 fd：EGL_BAD_NOT_INITIALIZED (0x3001)

CameraManager 预览分支改为 emit fd，VideoGLWidget 加 `setDmaBuf` + `createEglImage`。

第一版 `createEglImage` 用 `eglGetDisplay(EGL_DEFAULT_DISPLAY)` 取 dpy，`EGL_NO_CONTEXT` 做 ctx：

```cpp
EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
m_image = s_eglCreateImageKHR(dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, ...);
```

**结果**：`eglErr=0x3001` = `EGL_NOT_INITIALIZED`。

**原因**：`eglGetDisplay(EGL_DEFAULT_DISPLAY)` 拿到的是没 init 的 display。Qt eglfs 已 init 过自己的 display，但我们没取到它。

**解决**：改用 `eglGetCurrentDisplay()`（Qt 已 makeCurrent 的 display）。

### 9.6 M2 接 fd：EGL_BAD_PARAMETER (0x300c) —— 最大的弯路

改 `eglGetCurrentDisplay()` 后，错误码变成 `0x300c`。

**严重误判**：我把 `0x300c` 当成了 `EGL_BAD_RESOURCE`，编造了"Mali 拒收 rkisp exporter 的 dma_buf"的理论，甚至写了 VO 兜底方案。**这是完全错误的**——`EGL_BAD_RESOURCE` 这个常量在 EGL 标准里根本不存在：

```c
#define EGL_BAD_PARAMETER  0x300C   // egl.h:77
```

`0x300c` 是 `EGL_BAD_PARAMETER`，意思是"参数本身无效"，不是"资源拒收"。

**教训**：遇到错误码先查头文件确认常量名，不要凭记忆猜。

### 9.7 逐项排查 EGL_BAD_PARAMETER

#### 尝试 1：stride/height 对齐

dma_buf size = 3,133,440。反推：
- 1920 × 1088 × 1.5 = 3,133,440 ✓（stride=1920, bufH=1088）
- rockit `u32VirHeight` 报 1080（撒谎），dma_buf 实际按 1088 分配

试 `EGL_HEIGHT=1080` 和 `EGL_HEIGHT=1088`，都仍 0x300c。

#### 尝试 2：modifier = DRM_FORMAT_MOD_LINEAR

加 `EGL_DMA_BUF_PLANE0_MODIFIER_LO/HI = 0` + `PLANE1_MODIFIER_LO/HI = 0`，仍 0x300c。去掉 modifier 也 0x300c。

#### 尝试 3：ctx 参数

spec 要求 `EGL_LINUX_DMA_BUF_EXT` target 时 `ctx` 必须是 `EGL_NO_CONTEXT`。我之前误改成 `eglGetCurrentContext()`，改回 `EGL_NO_CONTEXT`。仍 0x300c。

#### 尝试 4：EGL_EXTS 确认

加 `eglQueryString(dpy, EGL_EXTENSIONS)` 打印。**确认 `EGL_EXT_image_dma_buf_import` 在当前 display 的扩展列表里**。target 应该被认。

#### 尝试 5：buffer 参数 = NULL（突破！）

spec 对 `eglCreateImageKHR` 的 `buffer` 参数描述有歧义。之前传 `(EGLClientBuffer)(uintptr_t)fd`，改成 `(EGLClientBuffer)0`（NULL）：

```cpp
m_image = s_eglCreateImageKHR(dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
                              (EGLClientBuffer)0, attr);
```

**结果**：`eglCreateImageKHR` 成功返回！不再报 `EGL_BAD_PARAMETER`。

**原因**：Mali driver 对 `EGL_LINUX_DMA_BUF_EXT` target 期望 `buffer=NULL`，fd 只在 attr list 的 `EGL_DMA_BUF_PLANE0_FD_EXT` 里传。传 fd 到 buffer 参数被 driver 视为"无效参数"。

### 9.8 黑屏问题：QOpenGLWidget 在 QGraphicsProxyWidget 下不能出图

EGLImage 成功后加 shader 画全屏 quad，但屏幕黑屏。

**诊断步骤**：
1. fragment shader 改成输出固定红色（不采样 texture）—— 仍黑屏
2. 发现 `paintGL` 在 `m_curFd < 0` 时 `return`，根本没画 quad —— 改成 fd<0 也画 quad
3. 但 `main.cpp` 用 `QGraphicsView::setRotation(270)` 旋转整个 UI，`QOpenGLWidget` 在 `QGraphicsProxyWidget` 内

**关键实验**：`main.cpp` 临时改成直接 `show()` 一个 `VideoGLWidget` 作为顶层窗口（不经过 QGraphicsView）。**红色矩形出现了**！

**结论**：`QOpenGLWidget` 在 `QGraphicsProxyWidget` 旋转下不能正常出图——Qt 的已知限制。GL widget 渲染到 FBO 后，QGraphicsProxyWidget 把它当 raster pixmap 合成，GL 内容丢失。

**解决**：`main.cpp` 去掉 `QGraphicsView` + `QGraphicsProxyWidget`，直接 `Widget::showFullScreen()`。旋转改为在 vertex shader 里做（`vec2 r = vec2(aPos.y, -aPos.x)` 270° 旋转）。

### 9.9 最终成功的 attr list

```cpp
EGLint attr[] = {
    EGL_WIDTH,                       m_curW,                  // 1920
    EGL_HEIGHT,                      (EGLint)bufH,            // 1088 (buffer 真实高度)
    EGL_LINUX_DRM_FOURCC_EXT,        (EGLint)fourcc,          // 0x3231564e = 'NV12'
    EGL_DMA_BUF_PLANE0_FD_EXT,       m_curFd,
    EGL_DMA_BUF_PLANE0_OFFSET_EXT,   0,
    EGL_DMA_BUF_PLANE0_PITCH_EXT,    (EGLint)stride,          // 1920
    EGL_DMA_BUF_PLANE1_FD_EXT,       m_curFd,
    EGL_DMA_BUF_PLANE1_OFFSET_EXT,   (EGLint)(stride * bufH), // 1920*1088 = 2088960
    EGL_DMA_BUF_PLANE1_PITCH_EXT,    (EGLint)stride,          // 1920
    EGL_NONE
};
// 无 modifier attr
// ctx = EGL_NO_CONTEXT
// buffer = (EGLClientBuffer)0
// dpy = eglGetCurrentDisplay()
```

### 9.10 性能验证（M2 完成）

零拷贝预览链路通了，摄像头画面在屏幕上显示。`top` 实测：

| 指标 | eglfs 基线（软件光栅） | 零拷贝（M2） | 变化 |
|---|---|---|---|
| VIRT | 1471 MB | 1557 MB | +86 MB（GL shader/EGL 资源） |
| RES | 98.4 MB | 96.4 MB | -2 MB |
| **%CPU** | **109.9%** | **47.1%** | **-62.8%（-57%）** |
| %MEM | 5.0% | 4.9% | -0.1% |

**CPU 降幅 57%，远超 plan §4.2 预期的 20-30%**。

### 9.11 当前未完成项

- **旋转**：vertex shader 里加了 270° 旋转（`vec2(aPos.y, -aPos.x)`），但方向还没调对。计划尝试硬件/内核层旋转方案（eglfs_kms 配置或 DRM plane rotation）
- **QGraphicsView 旋转已去掉**：`main.cpp` 直接 `Widget::showFullScreen()`，其他 page 的 Qt 控件目前不旋转。长期方案待定（eglfs_kms 硬件旋转 / shader 旋转 / 其他）
- **诊断 print 未清理**：`EGL_EXTS` / `paintGL count` / `VI frame` 的 printf 还在代码里
- **bufH hardcode 1088**：应从 `RK_MPI_MB_GetSize` 反推（`size / (1.5 * stride)`）
- **OSD 未加**：M3，`QPainter(this)` 在 paintGL 里画 FPS/时间/录像时长
- **page5（录像预览）/ 播放页**：还没换 VideoGLWidget
- **未 commit**：M2 成果还没提交

### 9.12 关键教训总结

1. **错误码先查头文件**：`0x300c` 是 `EGL_BAD_PARAMETER` 不是 `EGL_BAD_RESOURCE`（后者不存在）。凭记忆猜错误码常量名会误导整个调试方向
2. **扩展名字符串查询不可靠**：Mali wayland 变体有函数指针但不一定在所有 display 上声明字符串。必须用真调用验证
3. **`eglGetDisplay(EGL_DEFAULT_DISPLAY)` 在 Qt 应用里不能用**：Qt 已 init 过自己的 display，再 init 默认 display 会失败。用 `eglGetCurrentDisplay()` 取 Qt 的 display
4. **`EGL_LINUX_DMA_BUF_EXT` 的 buffer 参数传 NULL**：Mali driver 期望 fd 只在 attr list 里传，buffer 参数为 NULL
5. **`QOpenGLWidget` 不能在 `QGraphicsProxyWidget` 下出图**：Qt 已知限制。GL widget 必须直接作为顶层或普通 layout 子widget，不能经 QGraphicsView 旋转
6. **rockit `u32VirHeight` 不可信**：报 1080，但 dma_buf 实际按 1088 分配（16 对齐）。用 `RK_MPI_MB_GetSize` 反推真实高度
7. **dma_buf size 反推几何**：`size = stride × bufH × 1.5`（NV12），用 `/sys/kernel/debug/dma_buf/bufinfo` 确认真实 size

### 9.13 当前代码状态快照

- `main.cpp`：直接 `Widget::showFullScreen()`，无 QGraphicsView 旋转
- `videoglwidget.h/cpp`：`VideoGLWidget` 子类，`setDmaBuf(fd,w,h,mbBlk)`，`createEglImage` 用正确的 attr list，`paintGL` 画全屏 quad + 采样 OES external texture，vertex shader 有 270° 旋转（方向待调）
- `cameramanager.h/cpp`：`capFinishedToUI(int fd, int w, int h, void* mbBlk, uint64_t pts)`，预览分支用 `RK_MPI_MB_Handle2Fd` 取 fd emit，VI 帧走 `pendingViFrames_` 延迟释放
- `widget.h/cpp`：`doShow(int fd, int w, int h, void* mbBlk, uint64_t pts)` 转发给 `ui->openGLWidget->setDmaBuf`，连 `frameConsumed` → `previewFrameDone` → `releaseViFrame` 释放链
- `widget.ui`：page0 的 `openGLWidget` 已提升为 `VideoGLWidget`
- `.pro`：`QT += opengl`，`LIBS += -lEGL -lGLESv2`
- 诊断 print 仍在代码中（`EGL_EXTS` / `paintGL count` / `VI frame`）

---

_最后更新：2026-07-04，M2 预览零拷贝链路打通，CPU -57%，旋转待硬件方案_