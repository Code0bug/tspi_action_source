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
    thread_ = std::thread(&MediaManager::listen, this);
    capture_thread_ = std::thread(&VideoManager::startCapture, &video_);
    key_thread_ = std::thread(&KeyManager::listen, &keys_);
}

void MediaManager::stop()
{
    running_ = false;
    cv_.notify_one();
    keys_.stop();
    video_.stopCapture();
    if (key_thread_.joinable())
        key_thread_.join();
    if (capture_thread_.joinable())
        capture_thread_.join();
    if (thread_.joinable())
        thread_.join();
}

void MediaManager::listen()
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

        if (last == CamStatus::Preview && cur == CamStatus::Photo) {
            video_.takePhoto();
            // 拍完照后状态重新变成预览
            status_.store(CamStatus::Preview);
            last = CamStatus::Preview;
        } else if (last == CamStatus::Preview && cur == CamStatus::Record) {
            video_.startRecord();
            last = CamStatus::Record;
        } else if (last == CamStatus::Record && cur == CamStatus::Preview) {
            video_.stopRecord();
            last = CamStatus::Preview;
        } else {
            status_.store(last);
        }
    }
}
