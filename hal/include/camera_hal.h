#ifndef __CAMERA_HAL_H__
#define __CAMERA_HAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <linux/videodev2.h>

#define V4L2_CAMERA_BUF_COUNT 10
#define FMT_NUM_PLANES 1

typedef struct {
    void *start;
    size_t length;
    int dmabuf_fd;
    int index;
    int64_t timestamp_us;
} camera_frame_t;

typedef struct {
    int fd;
    unsigned int bufcnt;
    enum v4l2_buf_type type;
    unsigned int fmt;
    unsigned int width;
    unsigned int height;
    unsigned int bytesperline;
    camera_frame_t fbuf[V4L2_CAMERA_BUF_COUNT];
} camera_t;

camera_t* camera_init(const char *device, uint32_t bufcnt, uint32_t width, uint32_t height);

/* Nonblocking dequeue. Poll ctx->fd for POLLIN; EAGAIN means no frame yet.
 * A dequeued frame belongs to the caller until camera_release_frame(). */
int camera_get_frame(camera_t *ctx, camera_frame_t *frame);

int camera_release_frame(camera_t *ctx, camera_frame_t *frame);

void camera_deinit(camera_t *ctx);

#ifdef __cplusplus
}
#endif

#endif
