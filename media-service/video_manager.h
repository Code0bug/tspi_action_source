#ifndef MEDIA_SERVICE_VIDEO_MANAGER_H
#define MEDIA_SERVICE_VIDEO_MANAGER_H

#include <camera_hal.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>

class MediaManager;

class VideoManager {
public:
    explicit VideoManager(MediaManager &media);
    ~VideoManager();

    VideoManager(const VideoManager &) = delete;
    VideoManager &operator=(const VideoManager &) = delete;

    void start();
    void stop();
    void run();
    void startCapture();
    void stopCapture();
    std::string takePhoto();
    void startRecord();
    std::string stopRecord();
    void encoderSize(uint32_t *width, uint32_t *height, uint32_t *stride) const;
    void releaseFrame(camera_frame_t &frame);

private:
    bool createSocket();
    int rga_process(int src_fd, int src_w, int src_h,
                    int src_ws, int src_hs, int dst_fd, void *dst_vir,
                    int dst_w, int dst_h, int dst_ws, int dst_fmt, int usage);

    static const int kOverlayCount = 3;

    MediaManager &media_;
    camera_t *camera_display = nullptr;
    camera_t *camera_photo = nullptr;
    int m_socket = -1;
    int overlay_fds[kOverlayCount] = {-1, -1, -1};
    uint32_t overlay_width = 0;
    uint32_t overlay_height = 0;
    uint32_t overlay_pitch = 0;
    uint32_t overlay_count = 0;

    bool running_ = true;
    bool capturing_ = true;
    std::atomic<bool> recording_{false};
    bool save_first_frame_ = false;
    char first_frame_path_[128] = {0};
    std::mutex mu_;
    std::condition_variable cv_;
};

#endif
