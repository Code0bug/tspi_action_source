#define _GNU_SOURCE
#include "ipc_socket.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
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

ssize_t ipc_send(int socket_fd, const void *data, size_t size, const int *fds, int nfds)
{
    struct iovec iov = {(void *)data, size};
    struct msghdr msg = {0};
    char *control = NULL;
    ssize_t n;
    int saved_errno;

    if ((size > 0 && data == NULL) || nfds < 0 || (nfds > 0 && fds == NULL)) {
        errno = EINVAL;
        return -1;
    }

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    if (nfds > 0) {
        size_t ctrl_len = CMSG_SPACE(sizeof(int) * (size_t)nfds);
        control = calloc(1, ctrl_len);
        if (control == NULL)
            return -1;
        msg.msg_control = control;
        msg.msg_controllen = ctrl_len;

        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int) * (size_t)nfds);
        memcpy(CMSG_DATA(cmsg), fds, sizeof(int) * (size_t)nfds);
    }

    n = sendmsg(socket_fd, &msg, MSG_NOSIGNAL);
    saved_errno = errno;
    free(control);
    errno = saved_errno;
    return n;
}

ssize_t ipc_recv(int socket_fd, void *data, size_t size, int *fds, int *nfds)
{
    struct iovec iov = {data, size};
    struct msghdr msg = {0};
    char *control = NULL;
    int max_fds;
    int got = 0;
    int invalid;
    ssize_t bytes;
    int saved_errno;
    int i;

    if (nfds == NULL || *nfds < 0 || (*nfds > 0 && fds == NULL)) {
        errno = EINVAL;
        return -1;
    }

    max_fds = *nfds;
    *nfds = 0;
    for (i = 0; i < max_fds; ++i)
        fds[i] = -1;

    if (max_fds > 0) {
        size_t ctrl_len = CMSG_SPACE(sizeof(int) * (size_t)max_fds);
        control = calloc(1, ctrl_len);
        if (control == NULL)
            return -1;
        msg.msg_control = control;
        msg.msg_controllen = ctrl_len;
    }

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    bytes = recvmsg(socket_fd, &msg, MSG_CMSG_CLOEXEC);
    if (bytes < 0) {
        saved_errno = errno;
        free(control);
        errno = saved_errno;
        return -1;
    }

    invalid = msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC);
    for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
         cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        size_t count;
        size_t j;

        if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS)
            continue;

        count = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
        for (j = 0; j < count; ++j) {
            int fd;
            memcpy(&fd, CMSG_DATA(cmsg) + j * sizeof(int), sizeof(fd));
            if (got < max_fds)
                fds[got++] = fd;
            else {
                close(fd);
                invalid = 1;
            }
        }
    }

    free(control);

    if (invalid) {
        for (i = 0; i < got; ++i)
            close(fds[i]);
        for (i = 0; i < max_fds; ++i)
            fds[i] = -1;
        errno = EPROTO;
        return -1;
    }

    *nfds = got;
    return bytes;
}
