#include "media_manager.h"

#include <csignal>

int main()
{
    // 先屏蔽信号，子线程继承该掩码，由主线程 sigwait 统一处理退出。
    sigset_t set;
    int sig = 0;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &set, nullptr);

    MediaManager media;
    media.start();

    sigwait(&set, &sig);

    media.stop();
    return 0;
}
