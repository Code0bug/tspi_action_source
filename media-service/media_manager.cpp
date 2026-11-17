#include "media_manager.h"

#include <media_service.h>

#include <cstdio>
#include <string>

MediaManager::MediaManager()
    : video_(*this)
    , audio_(*this)
    , keys_(*this)
    , service_(*this)
    , muxer_(*this)
{
}

MediaManager::~MediaManager()
{
    stop();
}

bool MediaManager::statusUpdate(CamStatus next)
{
    if (status_.load() == next)
        return false;
    status_.store(next);
    cv_.notify_one();
    return true;
}

CamStatus MediaManager::statusGet() const
{
    return status_.load();
}

void MediaManager::start()
{
    video_.start();
    audio_.start();
    keys_.start();
    uint32_t width = 0, height = 0, stride = 0;
    video_.encoderSize(&width, &height, &stride);
    muxer_.start(width, height, stride);
    if (!service_.start())
        std::fprintf(stderr, "media-service: D-Bus control unavailable\n");

    running_ = true;
    thread_ = std::thread(&MediaManager::run, this);
    capture_thread_ = std::thread(&VideoManager::run, &video_);
    audio_thread_ = std::thread(&AudioManager::run, &audio_);
    key_thread_ = std::thread(&KeyManager::run, &keys_);
    muxer_thread_ = std::thread(&Mp4Muxer::run, &muxer_);
}

void MediaManager::stop()
{
    running_ = false;
    cv_.notify_one();
    keys_.stop();
    video_.stop();
    audio_.stop();
    if (key_thread_.joinable())
        key_thread_.join();
    if (capture_thread_.joinable())
        capture_thread_.join();
    if (audio_thread_.joinable())
        audio_thread_.join();
    muxer_.stop();
    if (muxer_thread_.joinable())
        muxer_thread_.join();
    if (thread_.joinable())
        thread_.join();
    service_.stop();
}

void MediaManager::releaseVideoFrame(camera_frame_t &frame)
{
    video_.releaseFrame(frame);
}

void MediaManager::run()
{
    CamStatus last = CamStatus::Preview;

    while (running_) {
        std::unique_lock<std::mutex> lock(mu_);
        cv_.wait(lock, [&] {
            return !running_.load() || status_.load() != last;
        });
        if (!running_)
            break;
        CamStatus cur = status_.load();
        lock.unlock();

        // 预览状态，按键按下：拍照
        if (last == CamStatus::Preview && cur == CamStatus::Photo) {
            service_.emit(MEDIA_SERVICE_SIGNAL_PHOTO_STARTED);
            std::string path = video_.takePhoto();
            service_.emit(MEDIA_SERVICE_SIGNAL_PHOTO_SAVED, path.c_str());
            // 拍完照后状态重新变成预览
            status_.store(CamStatus::Preview);
            cur = status_.load(); // 重新更新 cur
        }
        // 预览状态，按键长按：录像
        else if (last == CamStatus::Preview && cur == CamStatus::Record) {
            video_.startRecord();
            audio_.startCapture();
            service_.emit(MEDIA_SERVICE_SIGNAL_RECORD_STARTED);
        }
        // 录像状态，按键按下：停止录像
        else if (last == CamStatus::Record && cur == CamStatus::Preview) {
            audio_.stopCapture();
            std::string path = video_.stopRecord();
            service_.emit(MEDIA_SERVICE_SIGNAL_RECORD_STOPPED);
            if (!path.empty())
                service_.emit(MEDIA_SERVICE_SIGNAL_VIDEO_SAVED, path.c_str());
        }
        // 预览状态，切到相册：停止采集
        else if (last == CamStatus::Preview && cur == CamStatus::Paused) {
            video_.stopCapture();
        }
        // 暂停状态，切到预览：开始采集
        else if (last == CamStatus::Paused && cur == CamStatus::Preview) {
            video_.startCapture();
        }
        last = cur;
    }
}
