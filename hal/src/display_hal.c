#define _GNU_SOURCE
#include "display_hal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static int get_crtc_and_connector(drm_info_t *ctx)
{
    int i, e, c;

    for (i = 0; i < ctx->res->count_connectors; i++) {
        drmModeConnector *conn = drmModeGetConnector(ctx->fd, ctx->res->connectors[i]);
        uint32_t crtc = 0;

        if (!conn)
            continue;
        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
            for (e = 0; !crtc && e < conn->count_encoders; e++) {
                drmModeEncoder *enc = drmModeGetEncoder(ctx->fd, conn->encoders[e]);

                if (!enc)
                    continue;
                for (c = 0; !crtc && c < ctx->res->count_crtcs; c++) {
                    if (enc->possible_crtcs & (1u << c))
                        crtc = ctx->res->crtcs[c];
                }
                drmModeFreeEncoder(enc);
            }
        }
        if (crtc) {
            ctx->conn = conn;
            ctx->conn_id = conn->connector_id;
            ctx->crtc_id = crtc;
            return 0;
        }
        drmModeFreeConnector(conn);
    }
    return -1;
}

static void get_property(int fd, drmModeObjectProperties *props,
                         const char *name, uint32_t *prop_id, uint64_t *value)
{
    drmModePropertyPtr property;
    uint32_t i;
    int found = 0;

    for (i = 0; i < props->count_props; i++) {
        property = drmModeGetProperty(fd, props->props[i]);
        if (property && !strcmp(property->name, name)) {
            if (prop_id)
                *prop_id = property->prop_id;
            if (value)
                *value = props->prop_values[i];
            found = 1;
        }
        drmModeFreeProperty(property);

        if (found)
            break;
    }
}

static int get_planes(drm_info_t *ctx)
{
    int index = -1;
    int c;
    uint32_t mask;
    uint32_t i;

    for (c = 0; c < ctx->res->count_crtcs; c++) {
        if (ctx->res->crtcs[c] == ctx->crtc_id) {
            index = c;
            break;
        }
    }
    if (index < 0)
        return -1;
    mask = 1u << index;
    ctx->primary_id = 0;
    ctx->overlay_id = 0;

    for (i = 0; i < ctx->plane_res->count_planes; i++) {
        uint64_t type = DRM_PLANE_TYPE_CURSOR;  // 默认值，找不到 type 时会被忽略
        drmModeObjectProperties *props = NULL;
        uint32_t plane_id = ctx->plane_res->planes[i];
        drmModePlane *plane = drmModeGetPlane(ctx->fd, plane_id);
        if (!plane)
            continue;

        if (plane->possible_crtcs & mask)
            props = drmModeObjectGetProperties(ctx->fd, plane_id, DRM_MODE_OBJECT_PLANE);

        if (props) {
            get_property(ctx->fd, props, "type", NULL, &type);
            if (type == DRM_PLANE_TYPE_PRIMARY && ctx->primary_id == 0) {
                ctx->primary_id = plane_id;
            } else if (type == DRM_PLANE_TYPE_OVERLAY && ctx->overlay_id == 0) {
                ctx->overlay_id = plane_id;
            }
            drmModeFreeObjectProperties(props);
        }
        drmModeFreePlane(plane);
    }

    if (ctx->primary_id == 0 || ctx->overlay_id == 0) {
        fprintf(stderr, "Cannot find primary or overlay plane\n");
        return -1;
    }

    return 0;
}

static int load_plane_prop(int fd, uint32_t plane_id, plane_prop_t *prop)
{
    drmModeObjectProperties *props;

    memset(prop, 0, sizeof(*prop));
    props = drmModeObjectGetProperties(fd, plane_id, DRM_MODE_OBJECT_PLANE);
    if (!props)
        return -1;

    prop->plane_id = plane_id;
    get_property(fd, props, "CRTC_ID", &prop->crtc_id, NULL);
    get_property(fd, props, "FB_ID", &prop->fb_id, NULL);
    get_property(fd, props, "CRTC_X", &prop->crtc_x, NULL);
    get_property(fd, props, "CRTC_Y", &prop->crtc_y, NULL);
    get_property(fd, props, "CRTC_W", &prop->crtc_w, NULL);
    get_property(fd, props, "CRTC_H", &prop->crtc_h, NULL);
    get_property(fd, props, "SRC_X", &prop->src_x, NULL);
    get_property(fd, props, "SRC_Y", &prop->src_y, NULL);
    get_property(fd, props, "SRC_W", &prop->src_w, NULL);
    get_property(fd, props, "SRC_H", &prop->src_h, NULL);
    drmModeFreeObjectProperties(props);

    if (!prop->crtc_id || !prop->fb_id ||
        !prop->crtc_x || !prop->crtc_y || !prop->crtc_w || !prop->crtc_h ||
        !prop->src_x || !prop->src_y || !prop->src_w || !prop->src_h) {
        fprintf(stderr, "Plane %u: missing properties\n", plane_id);
        return -1;
    }
    return 0;
}

int update_plane(drm_info_t *ctx, plane_type_t type, const buffer_t *buf)
{
    const plane_prop_t *prop;
    drmModeAtomicReq *req;
    int ret;

    switch (type) {
    case PLANE_PRIMARY: prop = &ctx->primary; break;
    case PLANE_OVERLAY: prop = &ctx->overlay; break;
    default:
        fprintf(stderr, "update_plane: invalid plane type %d\n", type);
        return -1;
    }

    req = drmModeAtomicAlloc();
    if (!req)
        return -1;

    drmModeAtomicAddProperty(req, prop->plane_id, prop->crtc_id, ctx->crtc_id);
    drmModeAtomicAddProperty(req, prop->plane_id, prop->fb_id, buf->fb_id);
    drmModeAtomicAddProperty(req, prop->plane_id, prop->crtc_x, 0);
    drmModeAtomicAddProperty(req, prop->plane_id, prop->crtc_y, 0);
    drmModeAtomicAddProperty(req, prop->plane_id, prop->crtc_w, ctx->width);
    drmModeAtomicAddProperty(req, prop->plane_id, prop->crtc_h, ctx->height);
    drmModeAtomicAddProperty(req, prop->plane_id, prop->src_x, 0);
    drmModeAtomicAddProperty(req, prop->plane_id, prop->src_y, 0);
    drmModeAtomicAddProperty(req, prop->plane_id, prop->src_w, (uint64_t)buf->width << 16);
    drmModeAtomicAddProperty(req, prop->plane_id, prop->src_h, (uint64_t)buf->height << 16);

    ret = drmModeAtomicCommit(ctx->fd, req, 0, NULL);
    drmModeAtomicFree(req);
    if (ret != 0) {
        fprintf(stderr, "update_plane %u: %s\n", prop->plane_id, strerror(errno));
        return -1;
    }
    return 0;
}

static void buffer_release(int fd, buffer_t *buf)
{
    struct drm_mode_destroy_dumb destroy = {0};

    if (buf->prime_fd >= 0)
        close(buf->prime_fd);
    if (buf->vaddr)
        munmap(buf->vaddr, buf->size);
    if (buf->fb_id)
        drmModeRmFB(fd, buf->fb_id);
    if (buf->handle) {
        destroy.handle = buf->handle;
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    }
    memset(buf, 0, sizeof(*buf));
    buf->prime_fd = -1;
}

int buffer_create(drm_info_t *ctx, buffer_t *buf, uint32_t format,
                  bool map, bool export_fd)
{
    int fd = ctx->fd;
    uint32_t width = ctx->width;
    uint32_t height = ctx->height;
    struct drm_mode_create_dumb create = {0};
    uint32_t handles[4] = {0};
    uint32_t pitches[4] = {0};
    uint32_t offsets[4] = {0};

    if (!buf || width == 0 || height == 0)
        return -1;

    memset(buf, 0, sizeof(*buf));
    buf->prime_fd = -1;

    create.width = width;
    switch (format) {
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_ARGB8888:
        create.height = height;
        create.bpp = 32;
        break;
    case DRM_FORMAT_NV12:
        create.height = height * 3 / 2;
        create.bpp = 8;
        break;
    default:
        fprintf(stderr, "buffer_create: unsupported format 0x%08x\n", format);
        return -1;
    }

    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
        fprintf(stderr, "CREATE_DUMB: %s\n", strerror(errno));
        return -1;
    }

    buf->width = width;
    buf->height = height;
    buf->format = format;
    buf->pitch = create.pitch;
    buf->handle = create.handle;
    buf->size = create.size;

    handles[0] = create.handle;
    pitches[0] = create.pitch;
    if (format == DRM_FORMAT_NV12) {
        handles[1] = create.handle;
        pitches[1] = create.pitch;
        offsets[1] = create.pitch * height;
        if (create.pitch < width || create.size < offsets[1] + offsets[1] / 2) {
            fprintf(stderr, "buffer_create: NV12 buffer too small, pitch %u size %llu\n",
                    create.pitch, (unsigned long long)create.size);
            goto fail;
        }
    }

    if (drmModeAddFB2(fd, width, height, format, handles, pitches, offsets,
                      &buf->fb_id, 0) != 0) {
        fprintf(stderr, "drmModeAddFB2 %ux%u: %s\n", width, height, strerror(errno));
        goto fail;
    }

    if (map) {
        struct drm_mode_map_dumb mreq = {0};

        mreq.handle = create.handle;
        if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq) < 0) {
            fprintf(stderr, "MAP_DUMB: %s\n", strerror(errno));
            goto fail;
        }
        buf->vaddr = mmap(NULL, create.size, PROT_READ | PROT_WRITE,
                         MAP_SHARED, fd, mreq.offset);
        if (buf->vaddr == MAP_FAILED) {
            fprintf(stderr, "mmap dumb: %s\n", strerror(errno));
            buf->vaddr = NULL;
            goto fail;
        }
    }

    if (export_fd) {
        if (drmPrimeHandleToFD(fd, create.handle, DRM_CLOEXEC | DRM_RDWR,
                               &buf->prime_fd) != 0) {
            fprintf(stderr, "drmPrimeHandleToFD: %s\n", strerror(errno));
            buf->prime_fd = -1;
            goto fail;
        }
    }

    return 0;

fail:
    buffer_release(fd, buf);
    return -1;
}

drm_info_t* display_init(const char *device) {

    drm_info_t *ctx = NULL;
    drmModeObjectProperties *props;
    drmModeAtomicReq *req;
    uint32_t prop_conn_crtc_id = 0;
    uint32_t prop_crtc_active = 0;
    uint32_t prop_crtc_mode_id = 0;
    uint32_t blob_id = 0;
    int ret;
    ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
        return NULL;

    ctx->fd = -1;
    ctx->res = NULL;
    ctx->plane_res = NULL;
    ctx->conn = NULL;
    ctx->crtc_id = 0;
    ctx->conn_id = 0;
    ctx->width = 0;
    ctx->height = 0;

    // open drm device
    ctx->fd = open(device, O_RDWR | O_CLOEXEC);
    if (ctx->fd < 0) {
        fprintf(stderr, "Cannot open drm device: %s\n", device);
        goto out;
    }

    // get all planes and atomic support
    if (drmSetClientCap(ctx->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) != 0 ||
        drmSetClientCap(ctx->fd, DRM_CLIENT_CAP_ATOMIC, 1) != 0) {
        fprintf(stderr, "drmSetClientCap: %s\n", strerror(errno));
        goto out;
    }

    ctx->res = drmModeGetResources(ctx->fd);
    if (!ctx->res) {
        fprintf(stderr, "drmModeGetResources failed\n");
        goto out;
    }

    ctx->plane_res = drmModeGetPlaneResources(ctx->fd);
    if (!ctx->plane_res) {
        fprintf(stderr, "drmModeGetPlaneResources failed\n");
        goto out;
    }

    // get a valid crtc and connector for display
    if (get_crtc_and_connector(ctx) != 0) {
        fprintf(stderr, "Cannot get crtc and connector\n");
        goto out;
    }

    // find primary and overlay planes for the crtc
    if (get_planes(ctx) != 0) {
        fprintf(stderr, "Cannot find primary or NV12 overlay plane\n");
        goto out;
    }

    // load property ids of primary and overlay planes
    if (load_plane_prop(ctx->fd, ctx->primary_id, &ctx->primary) != 0 ||
        load_plane_prop(ctx->fd, ctx->overlay_id, &ctx->overlay) != 0) {
        fprintf(stderr, "Cannot load plane properties\n");
        goto out;
    }

    // get screen size
    ctx->width = ctx->conn->modes[0].hdisplay;
    ctx->height = ctx->conn->modes[0].vdisplay;

    printf("display: width %u height %u conn_id %u crtc_id %u\n",
           ctx->width, ctx->height, ctx->conn_id, ctx->crtc_id);
    printf("display: primary_id %u overlay_id %u\n", ctx->primary_id, ctx->overlay_id);

    // connect connector to crtc, set mode and activate crtc
    props = drmModeObjectGetProperties(ctx->fd, ctx->conn_id, DRM_MODE_OBJECT_CONNECTOR);
    if (!props) {
        fprintf(stderr, "Cannot get connector properties\n");
        goto out;
    }
    get_property(ctx->fd, props, "CRTC_ID", &prop_conn_crtc_id, NULL);
    drmModeFreeObjectProperties(props);
    if (prop_conn_crtc_id == 0) {
        fprintf(stderr, "Cannot find connector property CRTC_ID\n");
        goto out;
    }

    props = drmModeObjectGetProperties(ctx->fd, ctx->crtc_id, DRM_MODE_OBJECT_CRTC);
    if (!props) {
        fprintf(stderr, "Cannot get crtc properties\n");
        goto out;
    }
    get_property(ctx->fd, props, "ACTIVE", &prop_crtc_active, NULL);
    get_property(ctx->fd, props, "MODE_ID", &prop_crtc_mode_id, NULL);
    drmModeFreeObjectProperties(props);
    if (prop_crtc_active == 0 || prop_crtc_mode_id == 0) {
        fprintf(stderr, "Cannot find crtc property ACTIVE or MODE_ID\n");
        goto out;
    }

    if (drmModeCreatePropertyBlob(ctx->fd, &ctx->conn->modes[0],
                                  sizeof(ctx->conn->modes[0]), &blob_id) != 0) {
        fprintf(stderr, "drmModeCreatePropertyBlob: %s\n", strerror(errno));
        goto out;
    }

    req = drmModeAtomicAlloc();
    if (!req) {
        drmModeDestroyPropertyBlob(ctx->fd, blob_id);
        goto out;
    }
    drmModeAtomicAddProperty(req, ctx->crtc_id, prop_crtc_active, 1);
    drmModeAtomicAddProperty(req, ctx->crtc_id, prop_crtc_mode_id, blob_id);
    drmModeAtomicAddProperty(req, ctx->conn_id, prop_conn_crtc_id, ctx->crtc_id);
    ret = drmModeAtomicCommit(ctx->fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
    drmModeAtomicFree(req);
    drmModeDestroyPropertyBlob(ctx->fd, blob_id);
    if (ret != 0) {
        fprintf(stderr, "modeset atomic commit: %s\n", strerror(errno));
        goto out;
    }

    return ctx;

out:
    if (ctx->conn)
        drmModeFreeConnector(ctx->conn);
    if (ctx->plane_res)
        drmModeFreePlaneResources(ctx->plane_res);
    if (ctx->res)
        drmModeFreeResources(ctx->res);
    if (ctx->fd >= 0)
        close(ctx->fd);
    free(ctx);
    return NULL;
}
