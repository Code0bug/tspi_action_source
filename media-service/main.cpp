#include "video_manager.h"
#include "service.h"

#include <csignal>
#include <cstdio>
#include <thread>

int main()
{
    // 先屏蔽信号，子线程继承该掩码，由主线程 sigwait 统一处理退出。
    sigset_t set;
    int sig = 0;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &set, nullptr);

    VideoManager video;
    video.start();
    std::thread captureThread(&VideoManager::startCapture, &video);

    Service service;
    if (!service.start())
        std::fprintf(stderr, "media-service: D-Bus control unavailable\n");

    sigwait(&set, &sig);

    service.stop();
    video.stopCapture();
    captureThread.join();
    return 0;
}
