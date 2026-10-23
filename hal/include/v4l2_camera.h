#ifndef __V4L2_CAMERA_H__
#define __V4L2_CAMERA_H__

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
    int export_fd;
    int index;
    int64_t timestamp_us;
} v4l2_camera_frame_t;

typedef struct {
    int fd;
    unsigned int bufcnt;
    enum v4l2_buf_type type;
    unsigned int fmt;
    unsigned int width;
    unsigned int height;
    unsigned int bytesperline;
    v4l2_camera_frame_t fbuf[V4L2_CAMERA_BUF_COUNT];
} v4l2_camera_t;

v4l2_camera_t* v4l2_camera_init(const char *device, uint32_t bufcnt, uint32_t width, uint32_t height);

int v4l2_camera_get_frame(v4l2_camera_t *ctx, v4l2_camera_frame_t *frame);

int v4l2_camera_release_frame(v4l2_camera_t *ctx, v4l2_camera_frame_t *frame);

void v4l2_camera_deinit(v4l2_camera_t *ctx);

#ifdef __cplusplus
}
#endif

#endif
