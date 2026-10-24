#include "camera_manager.h"

#include <chrono>
#include <cstdio>
#include <thread>

static const char *const kMainCameraDevice = "/dev/video0";
static const uint32_t kMainWidth = 3264;
static const uint32_t kMainHeight = 2448;
static const char *const kSelfCameraDevice = "/dev/video1";
static const uint32_t kSelfWidth = 1920;
static const uint32_t kSelfHeight = 1080;
static const uint32_t kBufCount = 4;

CameraManager::CameraManager()
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

CameraManager::~CameraManager()
{
    camera_deinit(camera_display);
    camera_deinit(camera_photo);
}

void CameraManager::run()
{
    if (camera_display == nullptr)
        return;

    while (!quit_) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cond_.wait(lock, [this] { return quit_ || capturing_; });
        }
        if (quit_)
            break;

        camera_frame_t frame;
        if (camera_get_frame(camera_display, &frame) != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        if (handler_)
            handler_(frame);
        camera_release_frame(camera_display, &frame);
    }
}

void CameraManager::quit()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        quit_ = true;
    }
    cond_.notify_all();
}

void CameraManager::startCapture()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        capturing_ = true;
    }
    cond_.notify_all();
}

void CameraManager::stopCapture()
{
    std::lock_guard<std::mutex> lock(mutex_);
    capturing_ = false;
}
