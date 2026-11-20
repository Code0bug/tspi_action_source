#ifndef MEDIA_SERVICE_AUDIO_MANAGER_H
#define MEDIA_SERVICE_AUDIO_MANAGER_H

#include <audio_hal.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

class MediaManager;

class AudioManager {
public:
    explicit AudioManager(MediaManager &media);
    ~AudioManager();

    AudioManager(const AudioManager &) = delete;
    AudioManager &operator=(const AudioManager &) = delete;

    void start();
    void stop();
    void captureRun();
    void startCapture();
    void stopCapture();
    void startPlayback();
    void playbackRun();

private:
    MediaManager &media_;
    audio_t *audio_capture = nullptr;
    std::atomic<bool> running_{true};
    std::atomic<bool> capturing_{false};
    std::mutex mtx_;
    std::condition_variable cv_;
    std::thread audio_thread;
};

#endif
