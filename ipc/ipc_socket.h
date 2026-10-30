#ifndef TSPI_IPC_SOCKET_H
#define TSPI_IPC_SOCKET_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TSPI_PREVIEW_SOCKET_PATH "/run/tspi-camera/preview.sock"

typedef struct tspi_preview_buf {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t count;
    uint32_t slot;
} tspi_preview_buf;

int ipc_server_create(const char *path);
int ipc_client_create(const char *path);
ssize_t ipc_send(int socket_fd, const void *data, size_t size, const int *fds, int nfds);
ssize_t ipc_recv(int socket_fd, void *data, size_t size, int *fds, int *nfds);

#ifdef __cplusplus
}
#endif

#endif
