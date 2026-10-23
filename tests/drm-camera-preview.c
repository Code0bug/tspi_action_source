/*
 * 摄像头预览：V4L2 取出 1920x1080 NV12，RGA 顺时针转 90° 并缩放到屏幕大小，
 * overlay 按 1:1 扫描。方向与 QT_QPA_EGLFS_ROTATION=90 一致。
 * 参数 1 为单缓冲，2 或省略为双缓冲。Ctrl+C 退出。
 *
 * 运行前停掉占用 /dev/dri/card0 的界面程序，本进程需要成为 DRM master。
 * 采集节点默认 /dev/video0，可用环境变量 V4L2_DEVICE 覆盖。
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <rga/im2d.h>
#include <rga/rga.h>

#include "v4l2_camera.h"

#define CAM_WIDTH 1920
#define CAM_HEIGHT 1080
#define SCAN_BUFS 2

struct dumb_fb {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t handle;
    uint32_t size;
    uint8_t *vaddr;
    uint32_t fb_id;
};

struct plane_prop {
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
};

struct scan_nv12 {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t handle;
    uint32_t size;
    uint32_t fb_id;
    int prime_fd;
};

static volatile sig_atomic_t g_stop;

static void on_stop(int sig)
{
    (void)sig;
    g_stop = 1;
}

static uint32_t get_property_id(int fd, drmModeObjectProperties *props,
                                const char *name)
{
    uint32_t i, id = 0;

    for (i = 0; i < props->count_props; i++) {
        drmModePropertyPtr property = drmModeGetProperty(fd, props->props[i]);

        if (property && strcmp(property->name, name) == 0)
            id = property->prop_id;
        drmModeFreeProperty(property);
        if (id)
            break;
    }
    return id;
}

static uint32_t get_plane_type(int fd, uint32_t plane_id)
{
    drmModeObjectProperties *props;
    uint32_t type = (uint32_t)-1;
    uint32_t i;

    props = drmModeObjectGetProperties(fd, plane_id, DRM_MODE_OBJECT_PLANE);
    if (!props)
        return type;

    for (i = 0; i < props->count_props; i++) {
        drmModePropertyRes *prop = drmModeGetProperty(fd, props->props[i]);

        if (prop && strcmp(prop->name, "type") == 0)
            type = (uint32_t)props->prop_values[i];
        drmModeFreeProperty(prop);
    }
    drmModeFreeObjectProperties(props);
    return type;
}

static int plane_has_format(const drmModePlane *plane, uint32_t format)
{
    uint32_t i;

    for (i = 0; i < plane->count_formats; i++) {
        if (plane->formats[i] == format)
            return 1;
    }
    return 0;
}

static int crtc_index_of(drmModeRes *res, uint32_t crtc_id)
{
    int i;

    for (i = 0; i < res->count_crtcs; i++) {
        if (res->crtcs[i] == crtc_id)
            return i;
    }
    return -1;
}

static int load_plane_prop(int fd, uint32_t plane_id, struct plane_prop *out)
{
    drmModeObjectProperties *props;

    props = drmModeObjectGetProperties(fd, plane_id, DRM_MODE_OBJECT_PLANE);
    if (!props)
        return -1;

    memset(out, 0, sizeof(*out));
    out->plane_id = plane_id;
    out->crtc_id = get_property_id(fd, props, "CRTC_ID");
    out->fb_id = get_property_id(fd, props, "FB_ID");
    out->crtc_x = get_property_id(fd, props, "CRTC_X");
    out->crtc_y = get_property_id(fd, props, "CRTC_Y");
    out->crtc_w = get_property_id(fd, props, "CRTC_W");
    out->crtc_h = get_property_id(fd, props, "CRTC_H");
    out->src_x = get_property_id(fd, props, "SRC_X");
    out->src_y = get_property_id(fd, props, "SRC_Y");
    out->src_w = get_property_id(fd, props, "SRC_W");
    out->src_h = get_property_id(fd, props, "SRC_H");
    drmModeFreeObjectProperties(props);

    if (!out->crtc_id || !out->fb_id || !out->crtc_w || !out->crtc_h ||
        !out->src_w || !out->src_h)
        return -1;
    return 0;
}

static void add_plane(drmModeAtomicReq *req, const struct plane_prop *prop,
                      uint32_t crtc_id, uint32_t fb_id,
                      uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                      uint32_t src_w, uint32_t src_h)
{
    drmModeAtomicAddProperty(req, prop->plane_id, prop->crtc_id, crtc_id);
    drmModeAtomicAddProperty(req, prop->plane_id, prop->fb_id, fb_id);
    drmModeAtomicAddProperty(req, prop->plane_id, prop->crtc_x, x);
    drmModeAtomicAddProperty(req, prop->plane_id, prop->crtc_y, y);
    drmModeAtomicAddProperty(req, prop->plane_id, prop->crtc_w, w);
    drmModeAtomicAddProperty(req, prop->plane_id, prop->crtc_h, h);
    drmModeAtomicAddProperty(req, prop->plane_id, prop->src_x, 0);
    drmModeAtomicAddProperty(req, prop->plane_id, prop->src_y, 0);
    drmModeAtomicAddProperty(req, prop->plane_id, prop->src_w,
                             (uint64_t)src_w << 16);
    drmModeAtomicAddProperty(req, prop->plane_id, prop->src_h,
                             (uint64_t)src_h << 16);
}

static int create_black_fb(int fd, struct dumb_fb *bo)
{
    struct drm_mode_create_dumb create = {0};
    struct drm_mode_map_dumb map = {0};

    create.width = bo->width;
    create.height = bo->height;
    create.bpp = 32;
    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
        fprintf(stderr, "CREATE_DUMB: %s\n", strerror(errno));
        return -1;
    }

    bo->pitch = create.pitch;
    bo->size = create.size;
    bo->handle = create.handle;
    if (drmModeAddFB(fd, bo->width, bo->height, 24, 32, bo->pitch,
                     bo->handle, &bo->fb_id) != 0) {
        fprintf(stderr, "drmModeAddFB: %s\n", strerror(errno));
        return -1;
    }

    map.handle = create.handle;
    if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
        fprintf(stderr, "MAP_DUMB: %s\n", strerror(errno));
        return -1;
    }

    bo->vaddr = mmap(0, create.size, PROT_READ | PROT_WRITE,
                     MAP_SHARED, fd, map.offset);
    if (bo->vaddr == MAP_FAILED) {
        fprintf(stderr, "mmap dumb: %s\n", strerror(errno));
        bo->vaddr = NULL;
        return -1;
    }
    memset(bo->vaddr, 0, bo->size);
    return 0;
}

static void destroy_dumb_fb(int fd, struct dumb_fb *bo)
{
    struct drm_mode_destroy_dumb destroy = {0};

    if (bo->fb_id)
        drmModeRmFB(fd, bo->fb_id);
    if (bo->vaddr)
        munmap(bo->vaddr, bo->size);
    if (bo->handle) {
        destroy.handle = bo->handle;
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    }
    memset(bo, 0, sizeof(*bo));
}

static int pick_display(int fd, drmModeRes *res,
                        drmModeConnector **conn_out, uint32_t *crtc_id)
{
    int i;

    for (i = 0; i < res->count_connectors; i++) {
        drmModeConnector *conn;
        drmModeEncoder *enc;
        uint32_t crtc = 0;
        int e, c;

        conn = drmModeGetConnector(fd, res->connectors[i]);
        if (!conn)
            continue;
        if (conn->connection != DRM_MODE_CONNECTED || conn->count_modes == 0) {
            drmModeFreeConnector(conn);
            continue;
        }

        if (conn->encoder_id) {
            enc = drmModeGetEncoder(fd, conn->encoder_id);
            if (enc) {
                crtc = enc->crtc_id;
                drmModeFreeEncoder(enc);
            }
        }

        for (e = 0; !crtc && e < conn->count_encoders; e++) {
            enc = drmModeGetEncoder(fd, conn->encoders[e]);
            if (!enc)
                continue;
            for (c = 0; c < res->count_crtcs; c++) {
                if (enc->possible_crtcs & (1u << c)) {
                    crtc = res->crtcs[c];
                    break;
                }
            }
            drmModeFreeEncoder(enc);
        }

        if (!crtc) {
            drmModeFreeConnector(conn);
            continue;
        }

        *conn_out = conn;
        *crtc_id = crtc;
        return 0;
    }
    return -1;
}

static int find_planes(int fd, drmModeRes *res, drmModePlaneRes *plane_res,
                       uint32_t crtc_id, uint32_t *primary_id, uint32_t *overlay_id)
{
    int index = crtc_index_of(res, crtc_id);
    uint32_t mask;
    uint32_t i;

    if (index < 0)
        return -1;
    mask = 1u << index;
    *primary_id = 0;
    *overlay_id = 0;

    for (i = 0; i < plane_res->count_planes; i++) {
        drmModePlane *plane = drmModeGetPlane(fd, plane_res->planes[i]);
        uint32_t type;

        if (!plane)
            continue;
        if ((plane->possible_crtcs & mask) == 0) {
            drmModeFreePlane(plane);
            continue;
        }

        type = get_plane_type(fd, plane->plane_id);
        if (type == DRM_PLANE_TYPE_PRIMARY && *primary_id == 0)
            *primary_id = plane->plane_id;
        if (type == DRM_PLANE_TYPE_OVERLAY && *overlay_id == 0 &&
            plane_has_format(plane, DRM_FORMAT_NV12))
            *overlay_id = plane->plane_id;
        drmModeFreePlane(plane);
    }

    if (*primary_id == 0 || *overlay_id == 0)
        return -1;
    return 0;
}

static struct scan_nv12 g_scan[SCAN_BUFS];

static void destroy_scan_nv12(int fd, struct scan_nv12 *bo)
{
    struct drm_mode_destroy_dumb destroy = {0};

    if (bo->prime_fd >= 0)
        close(bo->prime_fd);
    if (bo->fb_id)
        drmModeRmFB(fd, bo->fb_id);
    if (bo->handle) {
        destroy.handle = bo->handle;
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    }
    memset(bo, 0, sizeof(*bo));
    bo->prime_fd = -1;
}

static int create_scan_nv12(int fd, struct scan_nv12 *bo,
                            uint32_t width, uint32_t height)
{
    struct drm_mode_create_dumb create = {0};
    uint32_t handles[4] = {0};
    uint32_t pitches[4] = {0};
    uint32_t offsets[4] = {0};
    uint32_t y_size;

    memset(bo, 0, sizeof(*bo));
    bo->prime_fd = -1;

    create.width = width;
    create.height = height * 3 / 2;
    create.bpp = 8;
    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
        fprintf(stderr, "CREATE_DUMB NV12: %s\n", strerror(errno));
        return -1;
    }

    bo->width = width;
    bo->height = height;
    bo->pitch = create.pitch;
    bo->handle = create.handle;
    bo->size = create.size;
    y_size = create.pitch * height;
    if (create.pitch < width || create.size < y_size + y_size / 2) {
        fprintf(stderr, "NV12 dumb 太小 pitch %u size %llu\n",
                create.pitch, (unsigned long long)create.size);
        return -1;
    }

    handles[0] = handles[1] = create.handle;
    pitches[0] = pitches[1] = create.pitch;
    offsets[1] = y_size;
    if (drmModeAddFB2(fd, width, height, DRM_FORMAT_NV12,
                      handles, pitches, offsets, &bo->fb_id, 0) != 0) {
        fprintf(stderr, "drmModeAddFB2 NV12 %ux%u: %s\n",
                width, height, strerror(errno));
        return -1;
    }

    if (drmPrimeHandleToFD(fd, create.handle, DRM_CLOEXEC | DRM_RDWR,
                           &bo->prime_fd) != 0) {
        fprintf(stderr, "drmPrimeHandleToFD: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

static int rga_rotate_scale(int src_fd, int src_w, int src_h,
                            int src_ws, int src_hs, struct scan_nv12 *dst)
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

static int commit_req(int fd, drmModeAtomicReq *req, uint32_t flags, const char *what)
{
    int ret = drmModeAtomicCommit(fd, req, flags, NULL);

    if (ret != 0)
        fprintf(stderr, "%s: %s\n", what, strerror(errno));
    return ret;
}

int main(int argc, char **argv)
{
    int fd = -1;
    int ret = 1;
    int nbufs = 2;
    drmModeRes *res = NULL;
    drmModePlaneRes *plane_res = NULL;
    drmModeConnector *conn = NULL;
    drmModeAtomicReq *req = NULL;
    struct dumb_fb black = {0};
    struct plane_prop primary, overlay;
    uint32_t conn_id = 0, crtc_id = 0, primary_id = 0, overlay_id = 0;
    uint32_t mode_w, mode_h, blob_id = 0;
    uint32_t prop_active = 0, prop_mode_id = 0, prop_crtc_id = 0;
    drmModeObjectProperties *props = NULL;
    uint64_t nframes = 0;
    int slot = 0;
    int i;
    struct timespec fps_t0;
    const char *device;
    v4l2_camera_t *cam = NULL;

    if (argc > 2 || (argc == 2 && strcmp(argv[1], "1") != 0 &&
                     strcmp(argv[1], "2") != 0)) {
        fprintf(stderr, "用法: %s [1|2]\n  1 单缓冲\n  2 双缓冲（默认）\n",
                argv[0]);
        return 1;
    }
    if (argc == 2 && strcmp(argv[1], "1") == 0)
        nbufs = 1;

    device = getenv("V4L2_DEVICE");
    if (device == NULL)
        device = "/dev/video0";

    for (i = 0; i < SCAN_BUFS; i++)
        g_scan[i].prime_fd = -1;

    signal(SIGINT, on_stop);
    signal(SIGTERM, on_stop);

    fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "open /dev/dri/card0: %s\n", strerror(errno));
        return 1;
    }
    if (drmSetMaster(fd) != 0) {
        fprintf(stderr, "drmSetMaster: %s\n", strerror(errno));
        fprintf(stderr, "停掉正在显示的界面程序后再运行\n");
        goto out;
    }
    if (drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) != 0 ||
        drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1) != 0) {
        fprintf(stderr, "drmSetClientCap: %s\n", strerror(errno));
        goto out;
    }

    res = drmModeGetResources(fd);
    plane_res = drmModeGetPlaneResources(fd);
    if (!res || !plane_res) {
        fprintf(stderr, "drmModeGetResources failed\n");
        goto out;
    }
    if (pick_display(fd, res, &conn, &crtc_id) != 0) {
        fprintf(stderr, "没有已连接的显示器\n");
        goto out;
    }

    conn_id = conn->connector_id;
    mode_w = conn->modes[0].hdisplay;
    mode_h = conn->modes[0].vdisplay;
    if (find_planes(fd, res, plane_res, crtc_id, &primary_id, &overlay_id) != 0) {
        fprintf(stderr, "找不到 primary，或找不到支持 NV12 的 overlay\n");
        goto out;
    }
    if (load_plane_prop(fd, primary_id, &primary) != 0 ||
        load_plane_prop(fd, overlay_id, &overlay) != 0) {
        fprintf(stderr, "读取 plane 属性失败\n");
        goto out;
    }

    printf("connector %u crtc %u mode %ux%u primary %u overlay %u\n",
           conn_id, crtc_id, mode_w, mode_h, primary_id, overlay_id);

    black.width = mode_w;
    black.height = mode_h;
    if (create_black_fb(fd, &black) != 0)
        goto out;
    if ((mode_w & 1) || (mode_h & 1)) {
        fprintf(stderr, "屏幕 %ux%u 不是偶数，NV12 无法旋转缩放\n",
                mode_w, mode_h);
        goto out;
    }
    for (i = 0; i < nbufs; i++) {
        if (create_scan_nv12(fd, &g_scan[i], mode_w, mode_h) != 0)
            goto out;
    }
    printf("scan NV12 %ux%u pitch %u，%s\n",
           g_scan[0].width, g_scan[0].height, g_scan[0].pitch,
           nbufs == 1 ? "单缓冲" : "双缓冲");

    props = drmModeObjectGetProperties(fd, crtc_id, DRM_MODE_OBJECT_CRTC);
    if (!props)
        goto out;
    prop_active = get_property_id(fd, props, "ACTIVE");
    prop_mode_id = get_property_id(fd, props, "MODE_ID");
    drmModeFreeObjectProperties(props);
    props = NULL;

    props = drmModeObjectGetProperties(fd, conn_id, DRM_MODE_OBJECT_CONNECTOR);
    if (!props)
        goto out;
    prop_crtc_id = get_property_id(fd, props, "CRTC_ID");
    drmModeFreeObjectProperties(props);
    props = NULL;

    if (!prop_active || !prop_mode_id || !prop_crtc_id)
        goto out;
    if (drmModeCreatePropertyBlob(fd, &conn->modes[0],
                                  sizeof(conn->modes[0]), &blob_id) != 0) {
        fprintf(stderr, "drmModeCreatePropertyBlob: %s\n", strerror(errno));
        goto out;
    }

    req = drmModeAtomicAlloc();
    if (!req)
        goto out;
    drmModeAtomicAddProperty(req, crtc_id, prop_active, 1);
    drmModeAtomicAddProperty(req, crtc_id, prop_mode_id, blob_id);
    drmModeAtomicAddProperty(req, conn_id, prop_crtc_id, crtc_id);
    add_plane(req, &primary, crtc_id, black.fb_id, 0, 0, mode_w, mode_h,
              mode_w, mode_h);
    if (commit_req(fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, "modeset") != 0) {
        drmModeAtomicFree(req);
        req = NULL;
        goto out;
    }
    drmModeAtomicFree(req);
    req = NULL;

    cam = v4l2_camera_init(device, 4, CAM_WIDTH, CAM_HEIGHT);
    if (cam == NULL) {
        fprintf(stderr, "v4l2_camera_init %s failed\n", device);
        goto out;
    }
    printf("V4L2 %s %ux%u stride %u，RGA 转 90° 铺满 %ux%u，Ctrl+C 退出\n",
           device, cam->width, cam->height, cam->bytesperline, mode_w, mode_h);
    clock_gettime(CLOCK_MONOTONIC, &fps_t0);

    while (!g_stop) {
        v4l2_camera_frame_t frame;

        if (v4l2_camera_get_frame(cam, &frame) != 0)
            continue;

        if (frame.export_fd < 0 ||
            rga_rotate_scale(frame.export_fd, (int)cam->width, (int)cam->height,
                             (int)cam->bytesperline, (int)cam->height,
                             &g_scan[slot]) != 0) {
            v4l2_camera_release_frame(cam, &frame);
            break;
        }
        v4l2_camera_release_frame(cam, &frame);

        req = drmModeAtomicAlloc();
        if (!req)
            break;
        add_plane(req, &overlay, crtc_id, g_scan[slot].fb_id,
                  0, 0, mode_w, mode_h, mode_w, mode_h);
        if (commit_req(fd, req, 0, "overlay") != 0) {
            drmModeAtomicFree(req);
            req = NULL;
            break;
        }
        drmModeAtomicFree(req);
        req = NULL;
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

    req = drmModeAtomicAlloc();
    if (req) {
        drmModeAtomicAddProperty(req, overlay.plane_id, overlay.crtc_id, 0);
        drmModeAtomicAddProperty(req, overlay.plane_id, overlay.fb_id, 0);
        commit_req(fd, req, 0, "disable overlay");
        drmModeAtomicFree(req);
        req = NULL;
    }
    if (g_stop)
        ret = 0;

out:
    v4l2_camera_deinit(cam);
    if (req)
        drmModeAtomicFree(req);
    if (props)
        drmModeFreeObjectProperties(props);
    for (i = 0; i < SCAN_BUFS; i++)
        destroy_scan_nv12(fd, &g_scan[i]);
    destroy_dumb_fb(fd, &black);
    if (blob_id)
        drmModeDestroyPropertyBlob(fd, blob_id);
    if (conn)
        drmModeFreeConnector(conn);
    if (plane_res)
        drmModeFreePlaneResources(plane_res);
    if (res)
        drmModeFreeResources(res);
    if (fd >= 0) {
        drmDropMaster(fd);
        close(fd);
    }
    return ret;
}
