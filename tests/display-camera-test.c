/*
 * 摄像头预览：V4L2 取出 1920x1080 NV12，RGA 顺时针转 90° 并缩放到屏幕大小，
 * overlay 按 1:1 扫描。方向与 QT_QPA_EGLFS_ROTATION=90 一致。
 * 参数 1 为单缓冲，2 或省略为双缓冲。Ctrl+C 退出。
 *
 * 运行前停掉占用 /dev/dri/card0 的界面程序，本进程需要成为 DRM master。
 */
#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <rga/im2d.h>
#include <rga/rga.h>

#include "camera_hal.h"
#include "display_hal.h"

#define CAM_WIDTH  1920
#define CAM_HEIGHT 1080
#define SCAN_BUFS  2

static volatile sig_atomic_t g_stop;

static void on_stop(int sig)
{
    (void)sig;
    g_stop = 1;
}

static int rga_rotate_scale(int src_fd, int src_w, int src_h,
                            int src_ws, int src_hs, buffer_t *dst)
{
    rga_buffer_t src, out, pat;
    im_rect srect, drect, prect;
    IM_STATUS status;

    memset(&pat, 0, sizeof(pat));
    memset(&srect, 0, sizeof(srect));
    memset(&drect, 0, sizeof(drect));
    memset(&prect, 0, sizeof(prect));

    src = wrapbuffer_fd(src_fd, src_w, src_h, RK_FORMAT_YCbCr_420_SP,
                        src_ws, src_hs);
    out = wrapbuffer_fd(dst->prime_fd, (int)dst->width, (int)dst->height,
                        RK_FORMAT_YCbCr_420_SP, (int)dst->pitch,
                        (int)dst->height);
    srect.width = src_w;
    srect.height = src_h;
    drect.width = (int)dst->width;
    drect.height = (int)dst->height;

    status = improcess(src, out, pat, srect, drect, prect,
                       IM_HAL_TRANSFORM_ROT_90 | IM_SYNC);
    if (status != IM_STATUS_SUCCESS && status != IM_STATUS_NOERROR) {
        fprintf(stderr, "improcess rotate: %s (%d)\n",
                imStrError(status), status);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int nbufs = 2;
    buffer_t black = {0};
    buffer_t scan[SCAN_BUFS] = {0};
    int slot = 0;
    uint64_t nframes = 0;
    struct timespec fps_t0;
    camera_t *cam = NULL;
    drm_info_t *ctx;
    int i;

    if (argc > 2 || (argc == 2 && strcmp(argv[1], "1") != 0 &&
                     strcmp(argv[1], "2") != 0)) {
        fprintf(stderr, "用法: %s [1|2]\n  1 单缓冲\n  2 双缓冲（默认）\n",
                argv[0]);
        return 1;
    }
    if (argc == 2 && strcmp(argv[1], "1") == 0)
        nbufs = 1;

    signal(SIGINT, on_stop);
    signal(SIGTERM, on_stop);

    /* 1. 初始化 DRM 显示 */
    ctx = display_init("/dev/dri/card0");
    if (!ctx) {
        fprintf(stderr, "display_init failed\n");
        return 1;
    }

    /* 2. primary plane 显示黑色背景 */
    if (buffer_create(ctx, &black, DRM_FORMAT_XRGB8888, true, false) != 0) {
        fprintf(stderr, "buffer_create black failed\n");
        return 1;
    }
    memset(black.vaddr, 0, black.size);
    if (update_plane(ctx, PLANE_PRIMARY, &black) != 0) {
        fprintf(stderr, "update_plane primary failed\n");
        return 1;
    }

    /* 3. 创建 NV12 扫描缓冲（RGA 目标），需要导出 dma-buf fd */
    if ((ctx->width & 1) || (ctx->height & 1)) {
        fprintf(stderr, "屏幕 %ux%u 不是偶数，NV12 无法旋转缩放\n",
                ctx->width, ctx->height);
        return 1;
    }
    for (i = 0; i < nbufs; i++) {
        if (buffer_create(ctx, &scan[i], DRM_FORMAT_NV12, false, true) != 0) {
            fprintf(stderr, "buffer_create scan NV12 failed\n");
            return 1;
        }
    }
    printf("scan NV12 %ux%u pitch %u，%s\n",
           scan[0].width, scan[0].height, scan[0].pitch,
           nbufs == 1 ? "单缓冲" : "双缓冲");

    /* 4. 初始化摄像头 */
    cam = camera_init("/dev/video0", 4, CAM_WIDTH, CAM_HEIGHT);
    if (!cam) {
        fprintf(stderr, "camera_init /dev/video0 failed\n");
        return 1;
    }
    printf("V4L2 /dev/video0 %ux%u stride %u，RGA 转 90° 铺满 %ux%u，Ctrl+C 退出\n",
           cam->width, cam->height, cam->bytesperline,
           ctx->width, ctx->height);
    clock_gettime(CLOCK_MONOTONIC, &fps_t0);

    /* 5. 预览循环 */
    while (!g_stop) {
        camera_frame_t frame;

        if (camera_get_frame(cam, &frame) != 0)
            continue;

        if (frame.dmabuf_fd < 0 ||
            rga_rotate_scale(frame.dmabuf_fd, (int)cam->width, (int)cam->height,
                             (int)cam->bytesperline, (int)cam->height,
                             &scan[slot]) != 0) {
            camera_release_frame(cam, &frame);
            break;
        }
        camera_release_frame(cam, &frame);

        if (update_plane(ctx, PLANE_OVERLAY, &scan[slot]) != 0)
            break;

        if (nbufs > 1)
            slot ^= 1;
        nframes++;

        if ((nframes % 60) == 0) {
            struct timespec now;
            double sec;

            clock_gettime(CLOCK_MONOTONIC, &now);
            sec = (now.tv_sec - fps_t0.tv_sec) +
                  (now.tv_nsec - fps_t0.tv_nsec) / 1e9;
            if (sec > 0.0)
                printf("frames %llu  %.1f fps\n",
                       (unsigned long long)nframes, 60.0 / sec);
            fps_t0 = now;
        }
    }

    camera_deinit(cam);
    return g_stop ? 0 : 1;
}
