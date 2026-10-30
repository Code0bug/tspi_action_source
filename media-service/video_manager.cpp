#include "video_manager.h"

#include "ipc_socket.h"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

#include <rga/im2d.h>
#include <rga/rga.h>

static const char *const kMainCameraDevice = "/dev/video0";
static const uint32_t kMainWidth = 3264;
static const uint32_t kMainHeight = 2448;
static const char *const kSelfCameraDevice = "/dev/video1";
static const uint32_t kSelfWidth = 1920;
static const uint32_t kSelfHeight = 1080;
static const uint32_t kBufCount = 4;

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

int VideoManager::rga_rotate_scale(int src_fd, int src_w, int src_h,
                                   int src_ws, int src_hs, int dst_fd)
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
    out = wrapbuffer_fd(dst_fd, (int)overlay_width, (int)overlay_height,
                        RK_FORMAT_YCbCr_420_SP, (int)overlay_pitch,
                        (int)overlay_height);
    srect.width = src_w;
    srect.height = src_h;
    drect.width = (int)overlay_width;
    drect.height = (int)overlay_height;

    status = improcess(src, out, pat, srect, drect, prect,
                       IM_HAL_TRANSFORM_ROT_90 | IM_SYNC);
    if (status != IM_STATUS_SUCCESS && status != IM_STATUS_NOERROR) {
        std::fprintf(stderr, "improcess rotate: %s (%d)\n",
                     imStrError(status), status);
        return -1;
    }
    return 0;
}

void VideoManager::startCapture()
{
    int slot = 0;

    if (camera_display == nullptr)
        return;

    capturing_ = true;
    while (capturing_) {
        camera_frame_t frame;
        if (camera_get_frame(camera_display, &frame) != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        if (frame.dmabuf_fd < 0 || overlay_fds[slot] < 0 ||
            rga_rotate_scale(frame.dmabuf_fd,
                             (int)camera_display->width,
                             (int)camera_display->height,
                             (int)camera_display->bytesperline,
                             (int)camera_display->height,
                             overlay_fds[slot]) != 0) {
            camera_release_frame(camera_display, &frame);
            continue;
        }
        camera_release_frame(camera_display, &frame);

        if (overlay_count > 1)
            slot ^= 1;
    }
}

void VideoManager::stopCapture()
{
    capturing_ = false;
}
