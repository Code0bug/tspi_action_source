#include "camera_manager.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <poll.h>

static const char *const kMainCameraDevice = "/dev/video0";
static const uint32_t kMainWidth = 3264;
static const uint32_t kMainHeight = 2448;
static const char *const kSelfCameraDevice = "/dev/video1";
static const uint32_t kSelfWidth = 1920;
static const uint32_t kSelfHeight = 1080;
static const uint32_t kBufCount = 4;

void CameraManager::start()
{
    if (!preview_.start())
        std::perror("CameraManager: preview socket unavailable");
    // 低分辨率 用于预览
    const char *previewDevice = std::getenv("TSPI_PREVIEW_CAMERA");
    if (!previewDevice || !*previewDevice) previewDevice = kSelfCameraDevice;
    camera_display = camera_init(previewDevice, kBufCount,
                                 kSelfWidth, kSelfHeight);
    if (camera_display == nullptr)
        std::fprintf(stderr, "CameraManager: open %s failed\n", previewDevice);

    // 高分辨率 用于拍照
    if (std::strcmp(previewDevice, kMainCameraDevice) != 0) {
        camera_photo = camera_init(kMainCameraDevice, kBufCount, kMainWidth, kMainHeight);
        if (camera_photo == nullptr)
            std::fprintf(stderr, "CameraManager: open %s failed\n", kMainCameraDevice);
    }
    capturing_ = true;
}

CameraManager::~CameraManager()
{
    camera_deinit(camera_display);
    camera_deinit(camera_photo);
}

void CameraManager::startCapture()
{
    while (capturing_) {
        preview_.pump();
        if (!camera_display) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        pollfd descriptor{camera_display->fd, POLLIN, 0};
        if (poll(&descriptor, 1, 20) <= 0 || !(descriptor.revents & POLLIN))
            continue;
        camera_frame_t frame;
        if (camera_get_frame(camera_display, &frame) != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        preview_.render(*camera_display, frame);
        camera_release_frame(camera_display, &frame);
    }
}

void CameraManager::stopCapture()
{
    capturing_ = false;
}
