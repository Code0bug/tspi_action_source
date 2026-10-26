#define _GNU_SOURCE
#include "camera_hal.h"
#include "ipc_socket.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define SOCK_PATH "/tmp/tspi-ipc-test.sock"
#define CAM_DEV "/dev/video1"
#define CAM_WIDTH 1920
#define CAM_HEIGHT 1080
#define CAM_BUFS 2

struct frame_msg {
    uint32_t width;
    uint32_t height;
    uint32_t length;
};

static int run_server(void)
{
    struct frame_msg msg;
    int conn;
    int recv_fd = -1;
    ssize_t n;

    unlink(SOCK_PATH);
    conn = ipc_server_create(SOCK_PATH);
    if (conn < 0) {
        perror("ipc_server_create");
        return 1;
    }

    n = ipc_rec(conn, &msg, sizeof(msg), &recv_fd);
    if (n < 0) {
        perror("ipc_rec");
        close(conn);
        return 1;
    }
    printf("server got %ux%u length=%u dmabuf_fd=%d\n",
           msg.width, msg.height, msg.length, recv_fd);

    if (recv_fd >= 0 && msg.length > 0) {
        void *p = mmap(NULL, msg.length, PROT_READ, MAP_SHARED, recv_fd, 0);
        if (p == MAP_FAILED) {
            perror("mmap dmabuf");
        } else {
            const unsigned char *y = p;
            printf("mmap ok, first Y bytes: %02x %02x %02x %02x\n",
                   y[0], y[1], y[2], y[3]);
            munmap(p, msg.length);
        }
        close(recv_fd);
    }

    close(conn);
    printf("server done\n");
    return 0;
}

static int run_client(const char *device)
{
    struct frame_msg msg;
    camera_t *cam;
    camera_frame_t frame;
    int sock = -1;
    int tries;

    for (tries = 0; tries < 50; tries++) {
        sock = ipc_client_create(SOCK_PATH);
        if (sock >= 0)
            break;
        usleep(100 * 1000);
    }
    if (sock < 0) {
        perror("ipc_client_create");
        return 1;
    }

    cam = camera_init(device, CAM_BUFS, CAM_WIDTH, CAM_HEIGHT);
    if (cam == NULL) {
        fprintf(stderr, "camera_init %s failed\n", device);
        close(sock);
        return 1;
    }

    if (camera_get_frame(cam, &frame) != 0) {
        fprintf(stderr, "camera_get_frame failed\n");
        camera_deinit(cam);
        close(sock);
        return 1;
    }

    msg.width = cam->width;
    msg.height = cam->height;
    msg.length = (uint32_t)frame.length;
    if (ipc_send(sock, &msg, sizeof(msg), frame.dmabuf_fd) < 0) {
        perror("ipc_send");
        camera_release_frame(cam, &frame);
        camera_deinit(cam);
        close(sock);
        return 1;
    }
    printf("client sent %ux%u dmabuf_fd=%d length=%u\n",
           msg.width, msg.height, frame.dmabuf_fd, msg.length);

    camera_release_frame(cam, &frame);
    camera_deinit(cam);
    close(sock);
    printf("client done\n");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "server") == 0)
        return run_server();
    if (argc >= 2 && strcmp(argv[1], "client") == 0)
        return run_client(argc > 2 ? argv[2] : CAM_DEV);

    fprintf(stderr, "usage: %s server|client [camera-device]\n", argv[0]);
    return 1;
}
