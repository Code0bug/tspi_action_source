#ifndef MEDIA_SERVICE_MEDIA_MANAGER_H
#define MEDIA_SERVICE_MEDIA_MANAGER_H

#include "key_manager.h"
#include "video_manager.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

enum class CamStatus {
    Preview,    // 预览
    Photo,      // 拍照
    Record,     // 录像
};

class MediaManager {
public:
    MediaManager();
    ~MediaManager();

    MediaManager(const MediaManager &) = delete;
    MediaManager &operator=(const MediaManager &) = delete;

    bool statusUpdate(CamStatus next);
    CamStatus statusGet() const;

    void start();
    void stop();

private:
    void listen();

    VideoManager video_;
    KeyManager keys_;

    std::atomic<CamStatus> status_{CamStatus::Preview};
    std::atomic<bool> running_{false};
    std::mutex mu_;
    std::condition_variable cv_;
    std::thread thread_;
    std::thread capture_thread_;
    std::thread key_thread_;
};

#endif
