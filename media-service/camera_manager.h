#ifndef MEDIA_SERVICE_CAMERA_MANAGER_H
#define MEDIA_SERVICE_CAMERA_MANAGER_H

#include <camera_hal.h>
#include <atomic>

class CameraManager {
public:
    CameraManager() = default;
    ~CameraManager();

    CameraManager(const CameraManager &) = delete;
    CameraManager &operator=(const CameraManager &) = delete;

    void start();
    void startCapture();
    void stopCapture();

private:
    bool createSocket();

    static const int kOverlayCount = 2;

    camera_t *camera_display = nullptr;
    camera_t *camera_photo = nullptr;
    int m_socket = -1;
    int overlay_fds[kOverlayCount] = {-1, -1};
    uint32_t overlay_width = 0;
    uint32_t overlay_height = 0;
    uint32_t overlay_pitch = 0;
    uint32_t overlay_count = 0;

    std::atomic<bool> capturing_{false};
};

#endif
