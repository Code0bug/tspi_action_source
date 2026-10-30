#include "camera_manager.h"

#include "ipc_socket.h"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

static const char *const kMainCameraDevice = "/dev/video0";
static const uint32_t kMainWidth = 3264;
static const uint32_t kMainHeight = 2448;
static const char *const kSelfCameraDevice = "/dev/video1";
static const uint32_t kSelfWidth = 1920;
static const uint32_t kSelfHeight = 1080;
static const uint32_t kBufCount = 4;

void CameraManager::start()
{
    // 低分辨率 用于预览
    camera_display = camera_init(kSelfCameraDevice, kBufCount,
                                 kSelfWidth, kSelfHeight);
    if (camera_display == nullptr)
        std::fprintf(stderr, "CameraManager: open %s failed\n", kSelfCameraDevice);

    // 高分辨率 用于拍照
    camera_photo = camera_init(kMainCameraDevice, kBufCount,
                               kMainWidth, kMainHeight);
    if (camera_photo == nullptr)
        std::fprintf(stderr, "CameraManager: open %s failed\n", kMainCameraDevice);

    // 创建 socket
    if (!createSocket())
        std::fprintf(stderr, "CameraManager: recv overlay fds failed\n");
}

bool CameraManager::createSocket()
{
    tspi_preview_buf msg = {};
    int fds[kOverlayCount];
    int nfds = kOverlayCount;

    if (mkdir("/run/tspi-camera", 0750) < 0 && errno != EEXIST)
        std::fprintf(stderr, "CameraManager: mkdir /run/tspi-camera: %s\n", std::strerror(errno));

    unlink(TSPI_PREVIEW_SOCKET_PATH);

    m_socket = ipc_server_create(TSPI_PREVIEW_SOCKET_PATH);
    if (m_socket < 0) {
        std::fprintf(stderr, "CameraManager: preview socket %s: %s\n",
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

    std::printf("CameraManager: recv %u overlay fds %ux%u pitch %u\n",
                overlay_count, overlay_width, overlay_height, overlay_pitch);
                
    return true;
}

CameraManager::~CameraManager()
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

void CameraManager::startCapture()
{
    if (camera_display == nullptr)
        return;

    capturing_ = true;
    while (capturing_) {
        camera_frame_t frame;
        if (camera_get_frame(camera_display, &frame) != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        camera_release_frame(camera_display, &frame);
    }
}

void CameraManager::stopCapture()
{
    capturing_ = false;
}
