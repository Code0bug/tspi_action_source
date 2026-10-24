#ifndef MEDIA_SERVICE_CAMERA_MANAGER_H
#define MEDIA_SERVICE_CAMERA_MANAGER_H

#include <camera_hal.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>

class CameraManager {
public:
    // 帧只在回调期间有效，回调返回后立即归还给驱动。
    typedef std::function<void(const camera_frame_t &)> FrameHandler;

    // 打开失败时对象仍可构造，用 isOpen() 判断。
    CameraManager();
    ~CameraManager();

    CameraManager(const CameraManager &) = delete;
    CameraManager &operator=(const CameraManager &) = delete;

    // 预览相机打开成功即可用；拍照相机可能单独失败，见 isPhotoOpen()。
    bool isOpen() const { return camera_display != nullptr; }
    bool isPhotoOpen() const { return camera_photo != nullptr; }

    // 采集线程主循环，阻塞直到 quit()。由调用方放进线程运行。
    void run();
    void quit();

    // 开始/暂停采集，线程安全；暂停时不取帧，线程休眠。
    void startCapture();
    void stopCapture();
    bool isCapturing() const { return capturing_; }

    // 必须在 run() 启动前设置。
    void setFrameHandler(FrameHandler handler) { handler_ = std::move(handler); }

private:
    camera_t *camera_display = nullptr;
    camera_t *camera_photo = nullptr;
    FrameHandler handler_;

    std::mutex mutex_;
    std::condition_variable cond_;
    std::atomic<bool> capturing_{false};
    std::atomic<bool> quit_{false};
};

#endif
