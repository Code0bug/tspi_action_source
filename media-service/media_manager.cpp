#include "media_manager.h"

#include <media_service.h>

#include <cstdio>
#include <string>

MediaManager::MediaManager()
    : video_(*this)
    , audio_(*this)
    , keys_(*this)
    , service_(*this)
    , mp4_(*this)
{
}

MediaManager::~MediaManager()
{
}

bool MediaManager::statusUpdate(MediaStatus next)
{
    if (status_.load() == next)
        return false;
    status_.store(next);
    cv_.notify_one();
    return true;
}

MediaStatus MediaManager::statusGet() const
{
    return status_.load();
}

void MediaManager::mp4PathSet(const std::string &path)
{
    std::lock_guard<std::mutex> lock(mp4_mtx_);
    mp4_path_ = path;
}

std::string MediaManager::mp4PathGet()
{
    std::lock_guard<std::mutex> lock(mp4_mtx_);
    return mp4_path_;
}

void MediaManager::start()
{
    // TODO: start 统一添加返回条件
    video_.start();
    audio_.start();
    keys_.start();
    // TODO: 后续应使用环境变量导入
    mp4_.start(1920, 1080, 1920);
    if (!service_.start())
        std::fprintf(stderr, "media-service: D-Bus control unavailable\n");

    running_ = true;
    thread_ = std::thread(&MediaManager::run, this);
}

void MediaManager::stop()
{
    running_ = false;
    cv_.notify_one();
    keys_.stop();
    video_.stop();
    audio_.stop();
    mp4_.stop();
    service_.stop();
    thread_.join();
}

void MediaManager::releaseVideoFrame(camera_frame_t &frame)
{
    video_.releaseFrame(frame);
}

void MediaManager::run()
{
    MediaStatus last = MediaStatus::Preview;

    while (running_) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [&] {
            return !running_.load() || status_.load() != last;
        });
        if (!running_)
            break;
        MediaStatus cur = status_.load();
        lock.unlock();

        // 预览状态，按键按下：拍照
        if (last == MediaStatus::Preview && cur == MediaStatus::Photo) {
            service_.emit(MEDIA_SERVICE_SIGNAL_PHOTO_STARTED);
            std::string path = video_.takePhoto();
            service_.emit(MEDIA_SERVICE_SIGNAL_PHOTO_SAVED, path.c_str());
            // 拍完照后状态重新变成预览
            status_.store(MediaStatus::Preview);
            cur = status_.load(); // 重新更新 cur
        }
        // 预览状态，按键长按：录像
        else if (last == MediaStatus::Preview && cur == MediaStatus::Record) {
            video_.startRecord();
            audio_.startCapture();
            service_.emit(MEDIA_SERVICE_SIGNAL_RECORD_STARTED);
        }
        // 录像状态，按键按下：停止录像
        else if (last == MediaStatus::Record && cur == MediaStatus::Preview) {
            audio_.stopCapture();
            std::string path = video_.stopRecord();
            service_.emit(MEDIA_SERVICE_SIGNAL_RECORD_STOPPED);
            if (!path.empty())
                service_.emit(MEDIA_SERVICE_SIGNAL_VIDEO_SAVED, path.c_str());
        }
        // 预览状态，切到相册：停止采集
        else if (last == MediaStatus::Preview && cur == MediaStatus::Paused) {
            video_.stopCapture();
        }
        // 暂停状态，切到预览：开始采集
        else if (last == MediaStatus::Paused && cur == MediaStatus::Preview) {
            video_.startCapture();
        }
        // 暂停状态，点击录像列表：播放录像
        else if (last == MediaStatus::Paused && cur == MediaStatus::Playing) {
            std::string mp4_path = mp4PathGet();
            std::printf("MediaManager: play video %s\n", mp4_path.c_str());
            // 先启动下游播放线程，再启动解封装
            video_.startPlayback();
            audio_.startPlayback();
            mp4_.startDemux(mp4_path);
        }
        // 播放状态，退出播放页：停止解封装（自然播完时已结束，重复停止无副作用）
        else if (last == MediaStatus::Playing && cur == MediaStatus::Paused) {
            mp4_.stopDemux();
        }
        last = cur;
    }
}
