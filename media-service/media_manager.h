#ifndef MEDIA_SERVICE_MEDIA_MANAGER_H
#define MEDIA_SERVICE_MEDIA_MANAGER_H

#include "audio_manager.h"
#include "key_manager.h"
#include "mp4_muxer.h"
#include "mux_job.h"
#include "safe_queue.h"
#include "service.h"
#include "video_manager.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

enum class MediaStatus {
    Preview,    // 预览
    Photo,      // 拍照
    Record,     // 录像
    Paused,     // 采集暂停
    Playing,    // 播放录像
};

class MediaManager {
public:
    MediaManager();
    ~MediaManager();

    MediaManager(const MediaManager &) = delete;
    MediaManager &operator=(const MediaManager &) = delete;

    bool statusUpdate(MediaStatus next);
    MediaStatus statusGet() const;
    void mp4PathSet(const std::string &path);
    std::string mp4PathGet();

    void start();
    void stop();
    void releaseVideoFrame(camera_frame_t &frame);

    SafeQueue<MuxJob> mux_queue;

private:
    void run();

    VideoManager video_;
    AudioManager audio_;
    KeyManager keys_;
    Service service_;
    Mp4Muxer muxer_;

    std::atomic<MediaStatus> status_{MediaStatus::Preview};
    std::atomic<bool> running_{false};
    std::string mp4_path_;
    std::mutex mp4_mtx_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::thread thread_;
};

#endif
