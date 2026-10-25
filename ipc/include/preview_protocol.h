#ifndef TSPI_PREVIEW_PROTOCOL_H
#define TSPI_PREVIEW_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PREVIEW_SOCKET_PATH "/run/tspi-camera/preview.sock"
#define PREVIEW_BUFFER_COUNT 3
#define PREVIEW_MAGIC UINT32_C(0x54535049)
#define PREVIEW_VERSION 1

enum preview_message_type {
    PREVIEW_HELLO = 1, /* UI requests physical scanout dimensions. */
    PREVIEW_INIT,      /* One NV12 dma-buf per slot, sent exactly once. */
    PREVIEW_FRAME,
    PREVIEW_RELEASE
};

/* Local SOCK_SEQPACKET ABI, fixed-width fields, no pointers or implicit padding.
 * A new connection always owns a new pool. Releases identify both pool and frame.
 * NV12 has two planes in one fd; UV begins at pitch * height. */
typedef struct preview_packet {
    uint32_t magic, version, type, bytes;
    uint64_t generation, sequence, timestamp_us;
    uint32_t slot, width, height, pitch;
    uint32_t format, buffer_count, buffer_bytes, reserved;
} preview_packet;

preview_packet preview_message(uint32_t type);
/* 1: complete packet, 0: EAGAIN, -1: disconnect/error/protocol violation.
 * recv always initializes fds to -1; caller owns returned fds on success only.
 * Only INIT may contain rights, and it must contain exactly three descriptors. */
int preview_send(int socket_fd, const preview_packet *packet, const int *fds);
int preview_recv(int socket_fd, preview_packet *packet, int fds[PREVIEW_BUFFER_COUNT]);
const char *preview_socket_path(void);

#ifdef __cplusplus
}
#endif
#endif
