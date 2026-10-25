#include <preview_protocol.h>
#include <preview_queue.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#define CHECK(value) do { if (!(value)) { std::fprintf(stderr, "%s:%d: %s (errno %d)\n", __FILE__, __LINE__, #value, errno); std::abort(); } } while (0)

static int descriptorCount()
{
    DIR *directory = opendir("/proc/self/fd");
    CHECK(directory);
    int count = 0;
    while (readdir(directory)) ++count;
    closedir(directory);
    return count;
}

static void malformed(int socket, preview_packet packet, int fd, bool oversized = false)
{
    char bytes[sizeof(packet) + 1]{};
    std::memcpy(bytes, &packet, sizeof(packet));
    iovec io{bytes, sizeof(packet) + (oversized ? 1u : 0u)};
    union { cmsghdr align; char data[CMSG_SPACE(sizeof(int))]; } control{};
    msghdr message{};
    message.msg_iov = &io; message.msg_iovlen = 1;
    if (fd >= 0) {
        message.msg_control = control.data; message.msg_controllen = sizeof(control.data);
        cmsghdr *c = CMSG_FIRSTHDR(&message);
        c->cmsg_level = SOL_SOCKET; c->cmsg_type = SCM_RIGHTS; c->cmsg_len = CMSG_LEN(sizeof(int));
        std::memcpy(CMSG_DATA(c), &fd, sizeof(fd));
    }
    CHECK(sendmsg(socket, &message, MSG_NOSIGNAL) == ssize_t(io.iov_len));
}

static void protocol()
{
    int pair[2];
    CHECK(!socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, pair));
    preview_packet received{}, packet = preview_message(PREVIEW_INIT);
    int fds[3], imported[3];
    for (int &fd : fds) {
        char path[] = "/tmp/tspi-preview-test-XXXXXX";
        fd = mkstemp(path); CHECK(fd >= 0); unlink(path);
        CHECK(write(fd, "NV12", 4) == 4);
        CHECK(lseek(fd, 0, SEEK_SET) == 0);
    }
    packet.generation = 42; packet.width = 720; packet.height = 1280;
    CHECK(preview_send(pair[0], &packet, fds) == 1);
    CHECK(preview_recv(pair[1], &received, imported) == 1);
    CHECK(received.generation == 42 && received.width == 720 && received.height == 1280);
    for (int fd : imported) {
        CHECK(fd >= 0 && (fcntl(fd, F_GETFD) & FD_CLOEXEC));
        char bytes[4]; CHECK(read(fd, bytes, sizeof(bytes)) == 4 && !std::memcmp(bytes, "NV12", 4));
        close(fd);
    }
    int baseline = descriptorCount();
    packet = preview_message(PREVIEW_FRAME);
    for (int n = 0; n < 64; ++n) {
        malformed(pair[0], packet, fds[0]); // Rights are forbidden on per-frame messages.
        CHECK(preview_recv(pair[1], &received, imported) == -1 && errno == EPROTO);
        CHECK(descriptorCount() == baseline);
    }
    malformed(pair[0], packet, fds[0], true); // Truncation must also close received fds.
    CHECK(preview_recv(pair[1], &received, imported) == -1 && errno == EPROTO);
    packet = preview_message(PREVIEW_INIT);
    malformed(pair[0], packet, fds[0]); // Missing two descriptors.
    CHECK(preview_recv(pair[1], &received, imported) == -1 && errno == EPROTO);
    packet = preview_message(PREVIEW_FRAME); ++packet.version;
    malformed(pair[0], packet, -1);
    CHECK(preview_recv(pair[1], &received, imported) == -1 && errno == EPROTO);
    packet = preview_message(PREVIEW_FRAME); ++packet.bytes;
    malformed(pair[0], packet, -1);
    CHECK(preview_recv(pair[1], &received, imported) == -1 && errno == EPROTO);
    CHECK(descriptorCount() == baseline);
    CHECK(preview_recv(pair[1], &received, imported) == 0);
    packet = preview_message(PREVIEW_FRAME);
    unsigned sent = 0;
    for (;;) {
        packet.sequence = sent;
        int result = preview_send(pair[0], &packet, nullptr);
        CHECK(result >= 0);
        if (!result) break;
        ++sent;
    }
    CHECK(sent > 0);
    for (unsigned i = 0; i < sent; ++i) {
        CHECK(preview_recv(pair[1], &received, imported) == 1 && received.sequence == i);
        for (int fd : imported) CHECK(fd == -1);
    }
    CHECK(preview_recv(pair[1], &received, imported) == 0);
    close(pair[1]);
    CHECK(preview_send(pair[0], &packet, nullptr) == -1); // No SIGPIPE termination.
    close(pair[0]);
    for (int fd : fds) close(fd);
}

static void lifetime()
{
    PreviewQueue queue;
    int dropped;
    CHECK(queue.offer(0, 10, dropped) && dropped == -1);
    queue.submitted(queue.next());
    CHECK(!queue.offer(0, 11, dropped)); // In-flight is unavailable before page flip.
    CHECK(queue.offer(1, 12, dropped) && dropped == -1);
    CHECK(queue.offer(2, 13, dropped) && dropped == 1); // Waiting is replaced immediately.
    CHECK(queue.sequence(dropped) == 12);
    CHECK(queue.complete() == -1 && queue.current() == 0);
    CHECK(!queue.offer(0, 14, dropped)); // The scanned frame still belongs to DRM.
    queue.submitted(queue.next());
    CHECK(queue.current() == 0);
    CHECK(!queue.offer(0, 14, dropped)); // Successful commit is insufficient.
    CHECK(queue.complete() == 0 && queue.current() == 2);
    CHECK(queue.offer(0, 15, dropped)); // Old scanout is writable only now.
    queue.submitted(queue.current()); // UI-only commit keeps the video frame pinned.
    CHECK(queue.complete() == -1 && queue.current() == 2 && queue.waiting() == 0);
    queue.discardWaiting();
    queue.submitted(-1); // Disconnect: disabling the video plane requires a flip too.
    CHECK(!queue.offer(2, 16, dropped));
    CHECK(queue.complete() == 2 && queue.current() == -1);
    CHECK(queue.offer(2, 16, dropped));
    CHECK(!queue.offer(-1, 0, dropped) && !queue.offer(3, 0, dropped));

    PreviewQueue disconnectDuringFirstFlip;
    CHECK(disconnectDuringFirstFlip.offer(0, 1, dropped));
    disconnectDuringFirstFlip.submitted(0);
    disconnectDuringFirstFlip.discardWaiting();
    CHECK(disconnectDuringFirstFlip.complete() == -1 && disconnectDuringFirstFlip.current() == 0);
    disconnectDuringFirstFlip.submitted(-1);
    CHECK(disconnectDuringFirstFlip.complete() == 0);
}

int main()
{
    int baseline = descriptorCount();
    protocol();
    lifetime();
    CHECK(descriptorCount() == baseline);
    std::puts("preview protocol, backpressure, descriptor cleanup and scanout lifetime: passed");
}
