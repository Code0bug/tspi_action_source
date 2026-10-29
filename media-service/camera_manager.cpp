#include "camera_manager.h"

#include "ipc_socket.h"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

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
}

bool CameraManager::sendDisplayFds(int socket)
{
    if (socket < 0 || camera_display == nullptr)
        return false;

    tspi_preview_buf msg = {};
    msg.width = camera_display->width;
    msg.height = camera_display->height;
    msg.pitch = camera_display->bytesperline;
    msg.count = camera_display->bufcnt;
    for (unsigned int i = 0; i < camera_display->bufcnt; ++i) {
        msg.slot = i;
        if (ipc_send(socket, &msg, sizeof(msg), camera_display->fbuf[i].dmabuf_fd) < 0) {
            std::fprintf(stderr, "CameraManager: send display fd %u: %s\n",
                         i, std::strerror(errno));
            return false;
        }
    }
    std::fprintf(stderr, "CameraManager: sent %u preview fds %ux%u pitch %u\n",
                 msg.count, msg.width, msg.height, msg.pitch);
    return true;
}

CameraManager::~CameraManager()
{
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
