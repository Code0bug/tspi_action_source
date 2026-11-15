#ifndef MEDIA_SERVICE_AUDIO_MANAGER_H
#define MEDIA_SERVICE_AUDIO_MANAGER_H

#include <atomic>
#include <condition_variable>
#include <mutex>

class MediaManager;

class AudioManager {
public:
    explicit AudioManager(MediaManager &media);
    ~AudioManager();

    AudioManager(const AudioManager &) = delete;
    AudioManager &operator=(const AudioManager &) = delete;

    void start();
    void stop();
    void run();
    void startCapture();
    void stopCapture();

private:
    MediaManager &media_;
    std::atomic<bool> running_{true};
    std::atomic<bool> capturing_{false};
    std::mutex mu_;
    std::condition_variable cv_;
};

#endif
