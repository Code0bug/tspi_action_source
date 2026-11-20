#include "video_manager.h"

#include "ipc_socket.h"
#include "media_manager.h"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

#include <opencv2/imgcodecs.hpp>
#include <rga/im2d.h>
#include <rga/rga.h>

static const char *const kMainCameraDevice = "/dev/video0";
static const uint32_t kMainWidth = 3264;
static const uint32_t kMainHeight = 2448;
static const char *const kSelfCameraDevice = "/dev/video1";
static const uint32_t kSelfWidth = 1920;
static const uint32_t kSelfHeight = 1080;
static const uint32_t kBufCount = 4;

VideoManager::VideoManager(MediaManager &media)
    : media_(media)
{
}

void VideoManager::start()
{
    // 低分辨率 用于预览
    camera_display = camera_init(kSelfCameraDevice, kBufCount,
                                 kSelfWidth, kSelfHeight);
    if (camera_display == nullptr)
        std::fprintf(stderr, "VideoManager: open %s failed\n", kSelfCameraDevice);

    // 高分辨率 用于拍照
    camera_photo = camera_init(kMainCameraDevice, kBufCount,
                               kMainWidth, kMainHeight);
    if (camera_photo == nullptr)
        std::fprintf(stderr, "VideoManager: open %s failed\n", kMainCameraDevice);

    // 创建 socket
    if (!createSocket())
        std::fprintf(stderr, "VideoManager: recv overlay fds failed\n");

    capture_thread = std::thread(&VideoManager::captureRun, this);
}

bool VideoManager::createSocket()
{
    tspi_preview_buf msg = {};
    int fds[kOverlayCount];
    int nfds = kOverlayCount;

    if (mkdir("/run/tspi-camera", 0750) < 0 && errno != EEXIST)
        std::fprintf(stderr, "VideoManager: mkdir /run/tspi-camera: %s\n", std::strerror(errno));

    unlink(TSPI_PREVIEW_SOCKET_PATH);

    m_socket = ipc_server_create(TSPI_PREVIEW_SOCKET_PATH);
    if (m_socket < 0) {
        std::fprintf(stderr, "VideoManager: preview socket %s: %s\n",
                     TSPI_PREVIEW_SOCKET_PATH, std::strerror(errno));
        return false;
    }

    // socket 连接成功后，UI 会发送为 video 准备的显示 framebuffer dma-buf fd
    ipc_recv(m_socket, &msg, sizeof(msg), fds, &nfds);

    overlay_width = msg.width;
    overlay_height = msg.height;
    overlay_pitch = msg.pitch;
    overlay_count = msg.count;
    for (int i = 0; i < nfds; ++i)
        overlay_fds[i] = fds[i];

    std::printf("VideoManager: recv %u overlay fds %ux%u pitch %u\n",
                overlay_count, overlay_width, overlay_height, overlay_pitch);

    return true;
}

VideoManager::~VideoManager()
{
    for (int i = 0; i < kOverlayCount; ++i) {
        if (overlay_fds[i] >= 0)
            close(overlay_fds[i]);
    }
    if (m_socket >= 0)
        close(m_socket);
    camera_deinit(camera_display);
    camera_deinit(camera_photo);
}

int VideoManager::rga_process(int src_fd, int src_w, int src_h,
                              int src_ws, int src_hs, int dst_fd, void *dst_vir,
                              int dst_w, int dst_h, int dst_ws, int dst_fmt,
                              int usage)
{
    rga_buffer_t src, out, pat;
    im_rect srect, drect, prect;
    IM_STATUS status;

    std::memset(&pat, 0, sizeof(pat));
    std::memset(&srect, 0, sizeof(srect));
    std::memset(&drect, 0, sizeof(drect));
    std::memset(&prect, 0, sizeof(prect));

    src = wrapbuffer_fd(src_fd, src_w, src_h, RK_FORMAT_YCbCr_420_SP,
                        src_ws, src_hs);
    if (dst_fd >= 0)
        out = wrapbuffer_fd(dst_fd, dst_w, dst_h, dst_fmt, dst_ws, dst_h);
    else
        out = wrapbuffer_virtualaddr(dst_vir, dst_w, dst_h, dst_fmt, dst_ws,
                                     dst_h);
    srect.width = src_w;
    srect.height = src_h;
    drect.width = dst_w;
    drect.height = dst_h;

    status = improcess(src, out, pat, srect, drect, prect, usage);
    if (status != IM_STATUS_SUCCESS && status != IM_STATUS_NOERROR) {
        std::fprintf(stderr, "improcess: %s (%d)\n",
                     imStrError(status), status);
        return -1;
    }
    return 0;
}

void VideoManager::stop()
{
    {
        std::lock_guard<std::mutex> lock(mtx_);
        running_ = false;
    }
    cv_.notify_one();
    capture_thread.join();

    // 播放线程已 detach，这里只通知它退出
    playback_running_ = false;
}

void VideoManager::captureRun()
{
    int slot = 0;

    for (;;) {
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this] { return !running_ || capturing_; });
            if (!running_)
                return;
        }

        camera_frame_t frame;
        if (camera_get_frame(camera_display, &frame) != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        // std::printf("VideoManager: get frame %d\n", frame.index);

        // rga 图像处理：旋转 90 度 和 缩放为屏幕尺寸大小
        if (rga_process(frame.dmabuf_fd,
                        (int)camera_display->width,
                        (int)camera_display->height,
                        (int)camera_display->bytesperline,
                        (int)camera_display->height,
                        overlay_fds[slot], nullptr,
                        (int)overlay_width, (int)overlay_height,
                        (int)overlay_pitch, RK_FORMAT_YCbCr_420_SP,
                        IM_HAL_TRANSFORM_ROT_90 | IM_SYNC) != 0) {
            camera_release_frame(camera_display, &frame);
            continue;
        }

        if (recording_.load()) {
            if (save_first_frame_) { // 录像记录下第一帧图像，用于预览图显示
                save_first_frame_ = false;
                cv::Mat ui_frame(160, 160, CV_8UC3);
                if (rga_process(frame.dmabuf_fd,
                                (int)camera_display->width,
                                (int)camera_display->height,
                                (int)camera_display->bytesperline,
                                (int)camera_display->height,
                                -1, ui_frame.data,
                                160, 160, 160, RK_FORMAT_BGR_888,
                                IM_HAL_TRANSFORM_ROT_180 | IM_SYNC) == 0)
                    cv::imwrite(first_frame_path_, ui_frame);
            }
            MuxJob job = {};
            job.type = MuxJobType::Video;
            job.video = frame;
            media_.mux_queue.push(job);
        } else {
            camera_release_frame(camera_display, &frame);
        }

        // socket 发送给 UI 显示
        tspi_preview_buf msg = {};
        msg.slot = uint32_t(slot);
        ipc_send(m_socket, &msg, sizeof(msg), nullptr, 0);

        if (overlay_count > 1)
            slot = (slot + 1) % int(overlay_count);
    }
}

void VideoManager::startCapture()
{
    {
        std::lock_guard<std::mutex> lock(mtx_);
        capturing_ = true;
    }
    cv_.notify_one();
}

void VideoManager::stopCapture()
{
    std::lock_guard<std::mutex> lock(mtx_);
    capturing_ = false;
}

void VideoManager::startPlayback()
{
    // 解码器需在 demux 送码流前就绪，所以在创建线程前同步初始化
    if (mpp_decoder_init() != 0) {
        std::fprintf(stderr, "VideoManager: mpp_decoder_init failed\n");
        return;
    }

    playback_running_ = true;
    // detach：线程退出后自动回收，下次播放可重新创建
    std::thread playback_thread(&VideoManager::playbackRun, this);
    playback_thread.detach();
}

void VideoManager::playbackRun()
{
    while (playback_running_) {
        DecodedFrame frame;
        int ret = mpp_decoder_get_frame(&frame);
        if (ret == MPP_DEC_RES_ERR)
            break;
        if (ret == MPP_DEC_RES_NONE)
            continue;

        // 收到 eos，播放结束，退出循环
        if (frame.eos)
            break;

        // TODO: RGA 将解码帧转换到 overlay buffer，并通过 socket 通知 UI 显示
        mpp_decoder_release_frame(&frame);
    }

    playback_running_ = false;
    mpp_decoder_deinit();
}

std::string VideoManager::takePhoto()
{
    camera_frame_t frame;
    const int drop_frames = 5;
    char path[128];
    time_t now;
    struct tm tm_now;
    int fd;

    for (int i = 0; i < drop_frames; ++i) {
        if (camera_get_frame(camera_photo, &frame) != 0)
            return {};
        camera_release_frame(camera_photo, &frame);
    }
    if (camera_get_frame(camera_photo, &frame) != 0) {
        std::fprintf(stderr, "VideoManager: take photo get frame failed\n");
        return {};
    }

    now = time(nullptr);
    localtime_r(&now, &tm_now);
    std::strftime(path, sizeof(path), "/userdata/picture/%Y%m%d_%H%M%S.png",
                  &tm_now);

    cv::Mat bgr((int)camera_photo->height, (int)camera_photo->width, CV_8UC3);
    int ret = rga_process(frame.dmabuf_fd,
                          (int)camera_photo->width,
                          (int)camera_photo->height,
                          (int)camera_photo->bytesperline,
                          (int)camera_photo->height,
                          -1, bgr.data,
                          (int)camera_photo->width, (int)camera_photo->height,
                          (int)camera_photo->width, RK_FORMAT_BGR_888,
                          IM_HAL_TRANSFORM_ROT_180 | IM_SYNC);
    camera_release_frame(camera_photo, &frame);
    if (ret != 0)
        return {};

    if (!cv::imwrite(path, bgr)) {
        std::fprintf(stderr, "VideoManager: imwrite failed: %s\n", path);
        return {};
    }
    fd = open(path, O_RDONLY);
    if (fd >= 0) {
        fsync(fd);
        close(fd);
    }
    std::printf("taking photo: %s\n", path);
    return path;
}

void VideoManager::encoderSize(uint32_t *width, uint32_t *height, uint32_t *stride) const
{
    *width = camera_display->width;
    *height = camera_display->height;
    *stride = camera_display->bytesperline;
}

void VideoManager::releaseFrame(camera_frame_t &frame)
{
    camera_release_frame(camera_display, &frame);
}

void VideoManager::startRecord()
{
    char name[32];
    char video_path[128];
    time_t now;
    struct tm tm_now;

    if (mkdir("/userdata/video", 0750) < 0 && errno != EEXIST)
        std::fprintf(stderr, "VideoManager: mkdir /userdata/video: %s\n",
                     std::strerror(errno));
    if (mkdir("/userdata/firstFrame", 0750) < 0 && errno != EEXIST)
        std::fprintf(stderr, "VideoManager: mkdir /userdata/firstFrame: %s\n",
                     std::strerror(errno));

    now = time(nullptr);
    localtime_r(&now, &tm_now);
    std::strftime(name, sizeof(name), "%Y%m%d_%H%M%S", &tm_now);
    std::snprintf(video_path, sizeof(video_path), "/userdata/video/%s.mp4", name);
    std::snprintf(first_frame_path_, sizeof(first_frame_path_),
                  "/userdata/firstFrame/%s.jpg", name);

    MuxJob job = {};
    job.type = MuxJobType::Open;
    job.path = video_path;
    media_.mux_queue.push(job);
    save_first_frame_ = true;
    recording_ = true;
    std::printf("VideoManager: start video %s\n", video_path);
}

std::string VideoManager::stopRecord()
{
    recording_ = false;
    MuxJob job = {};
    job.type = MuxJobType::Close;
    media_.mux_queue.push(job);
    std::string path = first_frame_path_;
    first_frame_path_[0] = '\0';
    std::printf("VideoManager: stop video\n");
    return path;
}
