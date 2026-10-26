#define _GNU_SOURCE
#include "ipc_socket.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int ipc_server_create(const char *path)
{
    struct sockaddr_un address = {0};
    size_t path_len;
    int fd;
    int conn;
    int flags;
    int saved_errno;

    if (path == NULL || path[0] == '\0') {
        errno = EINVAL;
        return -1;
    }

    path_len = strlen(path);
    if (path_len >= sizeof(address.sun_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    address.sun_family = AF_UNIX;
    memcpy(address.sun_path, path, path_len + 1);

    // create socket
    fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return -1;

    // bind socket to path
    if (bind(fd, (const struct sockaddr *)&address,
             (socklen_t)(offsetof(struct sockaddr_un, sun_path) + path_len + 1)) < 0) {
        saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return -1;
    }

    // listen for connections
    if (listen(fd, 1) < 0) {
        saved_errno = errno;
        close(fd);
        unlink(path);
        errno = saved_errno;
        return -1;
    }

    flags = fcntl(fd, F_GETFL);
    if (flags < 0 || fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) < 0) {
        saved_errno = errno;
        close(fd);
        unlink(path);
        errno = saved_errno;
        return -1;
    }

    conn = accept4(fd, NULL, NULL, SOCK_CLOEXEC);
    saved_errno = errno;
    close(fd);
    unlink(path);
    if (conn < 0) {
        errno = saved_errno;
        return -1;
    }

    return conn;
}

int ipc_client_create(const char *path)
{
    struct sockaddr_un address = {0};
    size_t path_len;
    int fd;
    int saved_errno;

    if (path == NULL || path[0] == '\0') {
        errno = EINVAL;
        return -1;
    }

    path_len = strlen(path);
    if (path_len >= sizeof(address.sun_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    address.sun_family = AF_UNIX;
    memcpy(address.sun_path, path, path_len + 1);

    // create socket
    fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return -1;

    // connect to server
    if (connect(fd, (const struct sockaddr *)&address,
                (socklen_t)(offsetof(struct sockaddr_un, sun_path) + path_len + 1)) < 0) {
        saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return -1;
    }

    return fd;
}

ssize_t ipc_send(int socket_fd, const void *data, size_t size, int send_fd)
{
    struct iovec iov = {(void *)data, size};    // 普通数据
    struct msghdr msg = {0};
    union {
        struct cmsghdr align;
        char data[CMSG_SPACE(sizeof(int))];
    } control = {0};    // 准备附加数据 这里就是文件描述符

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    msg.msg_control = control.data;
    msg.msg_controllen = sizeof(control.data);

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;   // 表述传递文件描述符
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &send_fd, sizeof(send_fd));

    return sendmsg(socket_fd, &msg, MSG_NOSIGNAL);
}

ssize_t ipc_rec(int socket_fd, void *data, size_t size, int *recv_fd)
{
    struct iovec iov = {data, size};
    struct msghdr msg = {0};
    union {
        struct cmsghdr align;
        char data[CMSG_SPACE(sizeof(int))];
    } control = {0};

    if (recv_fd == NULL) {
        errno = EINVAL;
        return -1;
    }
    *recv_fd = -1;

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control.data;
    msg.msg_controllen = sizeof(control.data);

    ssize_t bytes;
    bytes = recvmsg(socket_fd, &msg, MSG_CMSG_CLOEXEC);
    if (bytes < 0)
        return -1;

    int invalid = msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC);
    for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
         cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS)
            continue;

        size_t count = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
        for (size_t i = 0; i < count; ++i) {
            int fd;
            memcpy(&fd, CMSG_DATA(cmsg) + i * sizeof(int), sizeof(fd));
            if (*recv_fd < 0)
                *recv_fd = fd;
            else {
                close(fd);
                invalid = 1;
            }
        }
    }

    if (invalid) {
        if (*recv_fd >= 0)
            close(*recv_fd);
        *recv_fd = -1;
        errno = EPROTO;
        return -1;
    }
    return bytes;
}
