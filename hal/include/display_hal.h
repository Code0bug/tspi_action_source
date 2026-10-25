#ifndef __DISPLAY_HAL_H__
#define __DISPLAY_HAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

typedef struct {
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	uint32_t handle;
    uint32_t format;
	uint32_t size;
	uint8_t *vaddr;
	uint32_t fb_id;
    int prime_fd;
} buffer_t;

typedef struct {
    uint32_t plane_id;
    uint32_t crtc_id;
    uint32_t fb_id;
    uint32_t crtc_x;
    uint32_t crtc_y;
    uint32_t crtc_w;
    uint32_t crtc_h;
    uint32_t src_x;
    uint32_t src_y;
    uint32_t src_w;
    uint32_t src_h;
} plane_prop_t;

typedef enum {
    PLANE_PRIMARY,
    PLANE_OVERLAY,
} plane_type_t;

typedef struct {
    int fd;                     // drm 设备文件描述符
    drmModeRes *res;
    drmModePlaneRes *plane_res;
    drmModeConnector *conn;     // connector 对象
    uint32_t crtc_id;           // 使用的 crtc 对应的 id
    uint32_t conn_id;           // 使用的 connector 对应的 id
    uint32_t primary_id;        // 挂在 crtc 上的 primary plane id
    uint32_t overlay_id;        // 挂在 crtc 上的 overlay plane id
    plane_prop_t primary;       // primary plane 的属性 id
    plane_prop_t overlay;       // overlay plane 的属性 id
    uint32_t width;             // 屏幕宽度
	uint32_t height;            // 屏幕高度
} drm_info_t;

drm_info_t* display_init(const char *device);

int update_plane(drm_info_t *ctx, plane_type_t type, const buffer_t *buf);

int buffer_create(drm_info_t *ctx, buffer_t *buf, uint32_t format,
                  bool map, bool export_fd);

#ifdef __cplusplus
}
#endif

#endif
