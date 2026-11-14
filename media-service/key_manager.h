#ifndef MEDIA_SERVICE_KEY_MANAGER_H
#define MEDIA_SERVICE_KEY_MANAGER_H

#include <atomic>

class KeyManager {
public:
    KeyManager() = default;
    ~KeyManager();

    KeyManager(const KeyManager &) = delete;
    KeyManager &operator=(const KeyManager &) = delete;

    void start();
    void listen();
    void stop();

private:
    static const char *const kDevicePath;

    int fd_ = -1;
    std::atomic<bool> running_{false};
};

#endif
