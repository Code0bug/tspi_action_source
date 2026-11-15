#include "key_manager.h"

#include "media_manager.h"

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <unistd.h>

const char *const KeyManager::kDevicePath = "/dev/input/event3";

KeyManager::KeyManager(MediaManager &media)
    : media_(media)
{
}

KeyManager::~KeyManager()
{
    stop();
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}

void KeyManager::start()
{
    fd_ = open(kDevicePath, O_RDONLY | O_CLOEXEC);
    if (fd_ < 0)
        std::fprintf(stderr, "KeyManager: open %s failed: %s\n",
                     kDevicePath, std::strerror(errno));
}

void KeyManager::stop()
{
    running_ = false;
}

void KeyManager::run()
{
    struct pollfd pfd;
    struct input_event ev;
    bool pressed = false;
    std::chrono::steady_clock::time_point press_time;

    if (fd_ < 0)
        return;

    running_ = true;
    pfd.fd = fd_;
    pfd.events = POLLIN;

    while (running_) {
        int pret = poll(&pfd, 1, 200);
        if (pret < 0) {
            if (errno == EINTR)
                continue;
            std::fprintf(stderr, "KeyManager: poll: %s\n", std::strerror(errno));
            break;
        }
        if (pret == 0)
            continue;
        if ((pfd.revents & POLLIN) == 0)
            break;

        int ret = read(fd_, &ev, sizeof(ev));
        if (ret <= 0) {
            if (!running_)
                break;
            std::fprintf(stderr, "KeyManager: read failed\n");
            break;
        }
        if (ev.type != EV_KEY)
            continue;

        if (ev.value == 0) {
            press_time = std::chrono::steady_clock::now();
            pressed = true;
        } else if (ev.value == 1 && pressed) {
            pressed = false;
            int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::steady_clock::now() - press_time)
                                  .count();
            if (elapsed < 500) {    // 短按
                if (media_.statusGet() == CamStatus::Record) { // 如果此时正在录像，短按则停止录像回到预览
                    std::printf("KeyManager: short press, stop video\n");
                    media_.statusUpdate(CamStatus::Preview);
                } else {    // 拍照
                    std::printf("KeyManager: short press, take photo\n");
                    media_.statusUpdate(CamStatus::Photo);
                }
            } else {        // 长按-录像
                std::printf("KeyManager: long press, start video\n");
                media_.statusUpdate(CamStatus::Record);
            }
        }
    }
}
