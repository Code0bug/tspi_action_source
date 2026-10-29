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
    bool sendDisplayFds(int socket);
    void startCapture();
    void stopCapture();

private:
    camera_t *camera_display = nullptr;
    camera_t *camera_photo = nullptr;

    std::atomic<bool> capturing_{false};
};

#endif
