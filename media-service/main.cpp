#include "camera_manager.h"
#include "service.h"

#include "ipc_socket.h"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

int main()
{
    // 先屏蔽信号，子线程继承该掩码，由主线程 sigwait 统一处理退出。
    sigset_t set;
    int sig = 0;
    int preview_socket = -1;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &set, nullptr);

    if (mkdir("/run/tspi-camera", 0750) < 0 && errno != EEXIST)
        std::fprintf(stderr, "media-service: mkdir /run/tspi-camera: %s\n",
                     std::strerror(errno));
    unlink(TSPI_PREVIEW_SOCKET_PATH);
    preview_socket = ipc_server_create(TSPI_PREVIEW_SOCKET_PATH);
    if (preview_socket < 0)
        std::fprintf(stderr, "media-service: preview socket %s: %s\n",
                     TSPI_PREVIEW_SOCKET_PATH, std::strerror(errno));

    CameraManager camera;
    camera.start();
    if (preview_socket >= 0 && !camera.sendDisplayFds(preview_socket))
        std::fprintf(stderr, "media-service: send preview fds failed\n");
    std::thread captureThread(&CameraManager::startCapture, &camera);

    Service service;
    if (!service.start())
        std::fprintf(stderr, "media-service: D-Bus control unavailable\n");

    sigwait(&set, &sig);

    service.stop();
    camera.stopCapture();
    captureThread.join();
    if (preview_socket >= 0)
        close(preview_socket);
    return 0;
}
