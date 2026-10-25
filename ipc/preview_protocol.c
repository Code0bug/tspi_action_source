#define _GNU_SOURCE
#include "preview_protocol.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

_Static_assert(sizeof(preview_packet) == 72, "preview wire ABI changed");

const char *preview_socket_path(void)
{
    const char *path = getenv("TSPI_PREVIEW_SOCKET");
    return path && *path ? path : PREVIEW_SOCKET_PATH;
}

preview_packet preview_message(uint32_t type)
{
    preview_packet p = {0};
    p.magic = PREVIEW_MAGIC;
    p.version = PREVIEW_VERSION;
    p.type = type;
    p.bytes = sizeof(p);
    return p;
}

int preview_send(int fd, const preview_packet *p, const int *fds)
{
    struct iovec io = {(void *)p, sizeof(*p)};
    union { struct cmsghdr align; char data[CMSG_SPACE(sizeof(int) * 3)]; } control;
    struct msghdr msg = {0};
    ssize_t n;
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    if (p->type == PREVIEW_INIT) {
        struct cmsghdr *c;
        if (!fds) { errno = EINVAL; return -1; }
        memset(&control, 0, sizeof(control));
        msg.msg_control = control.data;
        msg.msg_controllen = sizeof(control.data);
        c = CMSG_FIRSTHDR(&msg);
        c->cmsg_level = SOL_SOCKET;
        c->cmsg_type = SCM_RIGHTS;
        c->cmsg_len = CMSG_LEN(sizeof(int) * 3);
        memcpy(CMSG_DATA(c), fds, sizeof(int) * 3);
    }
    do { n = sendmsg(fd, &msg, MSG_DONTWAIT | MSG_NOSIGNAL); } while (n < 0 && errno == EINTR);
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
    if (n == sizeof(*p)) return 1;
    if (n >= 0) errno = EPROTO;
    return -1;
}

int preview_recv(int fd, preview_packet *p, int fds[3])
{
    struct iovec io = {p, sizeof(*p)};
    /* Extra space lets us close unexpected rights, including malformed packets. */
    union { struct cmsghdr align; char data[CMSG_SPACE(sizeof(int) * 16)]; } control;
    struct msghdr msg = {0};
    int received[16], count = 0, invalid = 0;
    ssize_t n;
    for (int i = 0; i < 3; ++i) fds[i] = -1;
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = control.data;
    msg.msg_controllen = sizeof(control.data);
    do { n = recvmsg(fd, &msg, MSG_DONTWAIT | MSG_CMSG_CLOEXEC); } while (n < 0 && errno == EINTR);
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
    if (n < 0) return -1;
    for (struct cmsghdr *c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c)) {
        if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS) {
            size_t nf = (c->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            for (size_t i = 0; i < nf; ++i) {
                int value;
                memcpy(&value, (char *)CMSG_DATA(c) + i * sizeof(int), sizeof(int));
                if (count < 16) received[count++] = value;
                else { close(value); invalid = 1; }
            }
        }
    }
    if (n != sizeof(*p) || (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) ||
        p->magic != PREVIEW_MAGIC || p->version != PREVIEW_VERSION ||
        p->bytes != sizeof(*p) || p->type < PREVIEW_HELLO || p->type > PREVIEW_RELEASE ||
        count != (p->type == PREVIEW_INIT ? 3 : 0)) invalid = 1;
    if (invalid) {
        for (int i = 0; i < count; ++i) close(received[i]);
        errno = n == 0 ? ECONNRESET : EPROTO;
        return -1;
    }
    for (int i = 0; i < count; ++i) fds[i] = received[i];
    return 1;
}
