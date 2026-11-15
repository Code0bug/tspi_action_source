#include "media_manager.h"

MediaManager::MediaManager()
    : keys_(*this)
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
    keys_.start();

    running_ = true;
    thread_ = std::thread(&MediaManager::run, this);
    capture_thread_ = std::thread(&VideoManager::run, &video_);
    key_thread_ = std::thread(&KeyManager::run, &keys_);
}

void MediaManager::stop()
{
    running_ = false;
    cv_.notify_one();
    keys_.stop();
    video_.stop();
    if (key_thread_.joinable())
        key_thread_.join();
    if (capture_thread_.joinable())
        capture_thread_.join();
    if (thread_.joinable())
        thread_.join();
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
            video_.takePhoto();
            // 拍完照后状态重新变成预览
            status_.store(CamStatus::Preview);
            cur = status_.load(); // 重新更新 cur
        } 
        // 预览状态，按键长按：录像
        else if (last == CamStatus::Preview && cur == CamStatus::Record) {
            video_.startRecord();
        } 
        // 录像状态，按键松开：停止录像
        else if (last == CamStatus::Record && cur == CamStatus::Preview) {
            video_.stopRecord();
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
