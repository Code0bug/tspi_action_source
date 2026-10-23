#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stddef.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <linux/videodev2.h>

#include "v4l2_camera.h"

void v4l2_camera_deinit(v4l2_camera_t *ctx)
{
    unsigned int i;

    if (ctx == NULL)
        return;

    if (ctx->fd >= 0) {
        ioctl(ctx->fd, VIDIOC_STREAMOFF, &ctx->type);
        for (i = 0; i < ctx->bufcnt; i++) {
            if (ctx->fbuf[i].start != NULL && ctx->fbuf[i].start != MAP_FAILED)
                munmap(ctx->fbuf[i].start, ctx->fbuf[i].length);
            if (ctx->fbuf[i].export_fd >= 0)
                close(ctx->fbuf[i].export_fd);
        }
        close(ctx->fd);
    }
    free(ctx);
}

v4l2_camera_t* v4l2_camera_init(const char *device, uint32_t bufcnt, uint32_t width, uint32_t height)
{
    struct v4l2_capability     cap;
    struct v4l2_format         vfmt;
    struct v4l2_requestbuffers req;
    struct v4l2_buffer         buf;
    struct v4l2_exportbuffer   expbuf;
    v4l2_camera_t *ctx = NULL;
    unsigned int i;

    if (device == NULL || bufcnt == 0 || bufcnt > V4L2_CAMERA_BUF_COUNT)
        return NULL;

    ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
        return NULL;

    ctx->fd = -1;
    ctx->bufcnt = bufcnt;
    for (i = 0; i < V4L2_CAMERA_BUF_COUNT; i++)
        ctx->fbuf[i].export_fd = -1;
    ctx->fd = open(device, O_RDWR | O_CLOEXEC);
    if (ctx->fd < 0) {
        fprintf(stderr, "Cannot open device\n");
        goto FAIL;
    }

    if (ioctl(ctx->fd, VIDIOC_QUERYCAP, &cap) != 0) {
        fprintf(stderr, "Not v4l2 compatible\n");
        goto FAIL;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
        !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)) {
        fprintf(stderr, "Capture not supported\n");
        goto FAIL;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "Streaming IO Not Supported\n");
        goto FAIL;
    }

    memset(&vfmt, 0, sizeof(vfmt));
    vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
        vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    vfmt.fmt.pix.width = width;
    vfmt.fmt.pix.height = height;
    vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
    
    if (ioctl(ctx->fd, VIDIOC_S_FMT, &vfmt) != 0) {
        fprintf(stderr, "VIDIOC_S_FMT\n");
        goto FAIL;
    }

    if (ioctl(ctx->fd, VIDIOC_G_FMT, &vfmt) != 0) {
        fprintf(stderr, "VIDIOC_G_FMT\n");
        goto FAIL;
    }
    ctx->type = vfmt.type;
    if (ctx->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
        ctx->width = vfmt.fmt.pix_mp.width;
        ctx->height = vfmt.fmt.pix_mp.height;
        ctx->fmt = vfmt.fmt.pix_mp.pixelformat;
        ctx->bytesperline = vfmt.fmt.pix_mp.plane_fmt[0].bytesperline;
    } else {
        ctx->width = vfmt.fmt.pix.width;
        ctx->height = vfmt.fmt.pix.height;
        ctx->fmt = vfmt.fmt.pix.pixelformat;
        ctx->bytesperline = vfmt.fmt.pix.bytesperline;
    }
    if (ctx->bytesperline == 0)
        ctx->bytesperline = ctx->width;

    printf("get width %u height %u\n", ctx->width, ctx->height);

    memset(&req, 0, sizeof(req));
    req.count  = ctx->bufcnt;
    req.type   = ctx->type;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(ctx->fd, VIDIOC_REQBUFS, &req) != 0) {
        fprintf(stderr, "VIDIOC_REQBUFS\n");
        goto FAIL;
    }

    // query buffer
    buf.type = ctx->type;
    buf.memory = V4L2_MEMORY_MMAP;
    for (int i = 0; i < ctx->bufcnt; i++) {
        buf.index = i;
        struct v4l2_plane planes[FMT_NUM_PLANES];
        if (ctx->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
            buf.m.planes = planes;
            buf.length = FMT_NUM_PLANES;
        }
        if (ioctl(ctx->fd, VIDIOC_QUERYBUF, &buf) != 0) {
            fprintf(stderr, "VIDIOC_QUERYBUF\n");
            goto FAIL;
        }

        if (ctx->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
            ctx->fbuf[i].length = buf.m.planes[0].length;
            ctx->fbuf[i].start = mmap(NULL, buf.m.planes[0].length,
                                      PROT_READ | PROT_WRITE,
                                      MAP_SHARED,
                                      ctx->fd, buf.m.planes[0].m.mem_offset);

        } else {
            ctx->fbuf[i].length = buf.length;
            ctx->fbuf[i].start = mmap(NULL, buf.length,
                                      PROT_READ | PROT_WRITE,
                                      MAP_SHARED,
                                      ctx->fd, buf.m.offset);
        }

        // export dma-buf fd
        expbuf.type = ctx->type;
        expbuf.index = i;
        expbuf.flags = O_CLOEXEC;
        if (ioctl(ctx->fd, VIDIOC_EXPBUF, &expbuf) != 0) {
            fprintf(stderr, "VIDIOC_EXPBUF\n");
            goto FAIL;
        }
        ctx->fbuf[i].export_fd = expbuf.fd;
    }

    // queue buffer
    for (int i = 0; i < ctx->bufcnt; i++ ) {
        struct v4l2_plane planes[FMT_NUM_PLANES];

        buf = (struct v4l2_buffer) {0};
        buf.type    = ctx->type;
        buf.memory  = V4L2_MEMORY_MMAP;
        buf.index   = i;

        if (ctx->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
            buf.m.planes = planes;
            buf.length = FMT_NUM_PLANES;
        }

        if (ioctl(ctx->fd, VIDIOC_QBUF, &buf) != 0) {
            fprintf(stderr, "VIDIOC_QBUF %d\n", i);
            goto FAIL;
        }
    }

    // start streaming
    if (ioctl(ctx->fd, VIDIOC_STREAMON, &ctx->type) != 0) {
        fprintf(stderr, "VIDIOC_STREAMON\n");
        goto FAIL;
    }

    return ctx;

FAIL:
    v4l2_camera_deinit(ctx);
    return NULL;
}

int v4l2_camera_get_frame(v4l2_camera_t *ctx, v4l2_camera_frame_t *frame)
{
    struct v4l2_buffer buf;
    struct v4l2_plane planes[FMT_NUM_PLANES];

    if (ctx == NULL || frame == NULL)
        return -1;

    buf = (struct v4l2_buffer) {0};
    buf.type = ctx->type;
    buf.memory = V4L2_MEMORY_MMAP;
    if (ctx->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
        buf.m.planes = planes;
        buf.length = FMT_NUM_PLANES;
    }

    if (ioctl(ctx->fd, VIDIOC_DQBUF, &buf) != 0) {
        fprintf(stderr, "VIDIOC_DQBUF\n");
        return -1;
    }

    if (buf.index >= ctx->bufcnt) {
        fprintf(stderr, "buffer index out of bounds\n");
        return -1;
    }

    *frame = ctx->fbuf[buf.index];
    frame->index = (int)buf.index;
    frame->timestamp_us = (int64_t)buf.timestamp.tv_sec * 1000000LL
                        + (int64_t)buf.timestamp.tv_usec;
    return 0;
}

int v4l2_camera_release_frame(v4l2_camera_t *ctx, v4l2_camera_frame_t *frame)
{
    struct v4l2_buffer buf;
    struct v4l2_plane planes[FMT_NUM_PLANES];

    if (ctx == NULL || frame == NULL)
        return -1;
    if (frame->index < 0 || (unsigned int)frame->index >= ctx->bufcnt)
        return -1;

    buf = (struct v4l2_buffer) {0};
    buf.type = ctx->type;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = (unsigned int)frame->index;
    if (ctx->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
        buf.m.planes = planes;
        buf.length = FMT_NUM_PLANES;
    }

    if (ioctl(ctx->fd, VIDIOC_QBUF, &buf) != 0) {
        fprintf(stderr, "VIDIOC_QBUF\n");
        return -1;
    }

    return 0;
}
