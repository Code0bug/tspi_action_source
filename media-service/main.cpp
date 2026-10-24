#include "camera_manager.h"
#include "camera_service.h"

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

    CameraManager camera;
    if (!camera.isOpen())
        return 1;

    // 采集线程：程序启动后默认开启，UI 之后可通过 D-Bus 暂停/恢复。
    std::thread captureThread(&CameraManager::run, &camera);
    camera.startCapture();

    CameraService service(camera);
    if (!service.start())
        std::fprintf(stderr, "media-service: D-Bus control unavailable\n");

    sigwait(&set, &sig);

    service.stop();
    camera.quit();
    captureThread.join();
    return 0;
}
