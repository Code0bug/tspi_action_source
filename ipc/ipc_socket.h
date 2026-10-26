#ifndef TSPI_IPC_SOCKET_H
#define TSPI_IPC_SOCKET_H

#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int ipc_server_create(const char *path);
int ipc_client_create(const char *path);
ssize_t ipc_send(int socket_fd, const void *data, size_t size, int send_fd);
ssize_t ipc_rec(int socket_fd, void *data, size_t size, int *recv_fd);

#ifdef __cplusplus
}
#endif

#endif
