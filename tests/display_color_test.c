#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "display_hal.h"

/* 用纯色填充 XRGB8888 buffer：0x00RRGGBB */
static void fill_color(buffer_t *buf, uint32_t color)
{
    uint32_t *pixels = (uint32_t *)buf->vaddr;
    uint32_t count = buf->pitch / 4 * buf->height;

    for (uint32_t i = 0; i < count; i++)
        pixels[i] = color;
}

/* 填充竖条纹色卡：红 绿 蓝 白 */
static void fill_color_bars(buffer_t *buf)
{
    static const uint32_t colors[] = {
        0x00FF0000, /* 红 */
        0x0000FF00, /* 绿 */
        0x000000FF, /* 蓝 */
        0x00FFFFFF, /* 白 */
    };
    int bar_count = sizeof(colors) / sizeof(colors[0]);
    uint32_t *pixels = (uint32_t *)buf->vaddr;
    uint32_t stride = buf->pitch / 4;
    uint32_t bar_w = buf->width / bar_count;

    for (uint32_t y = 0; y < buf->height; y++) {
        for (uint32_t x = 0; x < buf->width; x++) {
            int idx = x / bar_w;
            if (idx >= bar_count)
                idx = bar_count - 1;
            pixels[y * stride + x] = colors[idx];
        }
    }
}

int main(int argc, char **argv)
{
    const char *device = argc > 1 ? argv[1] : "/dev/dri/card0";
    buffer_t buf;
    int ret;

    /* 1. 初始化 DRM 显示 */
    drm_info_t *ctx = display_init(device);
    if (!ctx) {
        fprintf(stderr, "display_init %s failed\n", device);
        return 1;
    }
    printf("screen: %ux%u\n", ctx->width, ctx->height);

    /* 2. 创建一个全屏 XRGB8888 dumb buffer，需要 mmap */
    ret = buffer_create(ctx, &buf, DRM_FORMAT_XRGB8888, true, false);
    if (ret != 0) {
        fprintf(stderr, "buffer_create failed\n");
        return 1;
    }
    printf("buffer: %ux%u pitch=%u fb_id=%u\n",
           buf.width, buf.height, buf.pitch, buf.fb_id);

    /* 3. 填充色卡并送 primary plane 显示 */
    fill_color_bars(&buf);
    ret = update_plane(ctx, PLANE_PRIMARY, &buf);
    if (ret != 0) {
        fprintf(stderr, "update_plane PRIMARY failed\n");
        return 1;
    }
    printf("color bars displayed on primary plane, hold 3s ...\n");
    sleep(3);

    /* 4. 换成纯蓝色再刷一次 */
    fill_color(&buf, 0x000000FF);
    ret = update_plane(ctx, PLANE_PRIMARY, &buf);
    if (ret != 0) {
        fprintf(stderr, "update_plane PRIMARY (blue) failed\n");
        return 1;
    }
    printf("solid blue displayed, hold 3s ...\n");
    sleep(3);

    printf("test done\n");
    return 0;
}
