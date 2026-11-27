#ifndef MEDIA_SERVICE_KEY_MANAGER_H
#define MEDIA_SERVICE_KEY_MANAGER_H

#include <atomic>
#include <thread>

class MediaManager;

class KeyManager {
public:
    explicit KeyManager(MediaManager &media);
    ~KeyManager();

    KeyManager(const KeyManager &) = delete;
    KeyManager &operator=(const KeyManager &) = delete;

    void start();
    void run();
    void stop();

private:
    static const char *const kDevicePath;

    MediaManager &media_;
    int fd_ = -1;
    std::atomic<bool> running_{false};
    std::thread key_thread;
};

#endif
