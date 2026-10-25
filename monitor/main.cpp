#include <sys/signalfd.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

using Clock = std::chrono::steady_clock;

struct Child {
    const char *name;
    pid_t pid = -1;
    unsigned delay = 1;
    Clock::time_point started{}, restart{};
    explicit Child(const char *value) : name(value) {}
};

static void launch(Child &child, const std::string &directory, const sigset_t &originalMask)
{
    std::string path = directory + "/" + child.name;
    pid_t parent = getpid();
    child.pid = fork();
    if (child.pid == 0) {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        if (getppid() != parent) _exit(1);
        sigprocmask(SIG_SETMASK, &originalMask, nullptr);
        execl(path.c_str(), child.name, static_cast<char *>(nullptr));
        std::fprintf(stderr, "monitor: exec %s: %s\n", path.c_str(), std::strerror(errno));
        _exit(127);
    }
    child.started = Clock::now();
    if (child.pid < 0) child.restart = child.started + std::chrono::seconds(child.delay);
    else std::fprintf(stderr, "monitor: started %s pid=%d\n", child.name, child.pid);
}

int main()
{
    char executable[4096];
    ssize_t count = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    if (count <= 0 || count >= ssize_t(sizeof(executable) - 1)) return 1;
    executable[count] = '\0';
    std::string directory(executable);
    directory.resize(directory.rfind('/'));
    sigset_t mask, originalMask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD); sigaddset(&mask, SIGINT); sigaddset(&mask, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &mask, &originalMask)) return 1;
    int signals = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
    if (signals < 0) return 1;
    Child children[] = {Child("media-service"), Child("camera-ui")};
    bool stopping = false, killed = false;
    Clock::time_point deadline{};
    for (;;) {
        auto now = Clock::now();
        for (Child &child : children) {
            if (child.pid > 0) {
                int status;
                pid_t result = waitpid(child.pid, &status, WNOHANG);
                if (result == child.pid) {
                    std::fprintf(stderr, "monitor: %s pid=%d exited status=%d\n", child.name, child.pid, status);
                    child.pid = -1;
                    if (now - child.started > std::chrono::seconds(30)) child.delay = 1;
                    child.restart = now + std::chrono::seconds(child.delay);
                    child.delay = std::min(child.delay * 2, 30u);
                }
            }
            if (!stopping && child.pid < 0 && now >= child.restart) launch(child, directory, originalMask);
        }
        if (stopping) {
            if (children[0].pid < 0 && children[1].pid < 0) break;
            if (!killed && now >= deadline) {
                for (Child &child : children) if (child.pid > 0) kill(child.pid, SIGKILL);
                killed = true;
            }
        }
        pollfd descriptor{signals, POLLIN, 0};
        poll(&descriptor, 1, 100);
        signalfd_siginfo event{};
        while (read(signals, &event, sizeof(event)) == sizeof(event)) {
            if (!stopping && (event.ssi_signo == SIGINT || event.ssi_signo == SIGTERM)) {
                stopping = true;
                deadline = Clock::now() + std::chrono::seconds(3);
                for (Child &child : children) if (child.pid > 0) kill(child.pid, SIGTERM);
            }
        }
    }
    close(signals);
    return 0;
}
