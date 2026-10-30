/*
 * 参考 qt5base/src/plugins/platforms/linuxfb/qlinuxfbdrmscreen.cpp 实现
 * qlinuxfbdrmscreen 中定义 QLinuxFbDevice 类，其继承 QKmsDevice 类
 * QLinuxFbDevice 类本质是对 DRM 设备的管理，QKmsDevice 是基类封装
 *
 * tspi_drmscreen 中定义 DrmDevice 类，替代 QLinuxFbDevice + QKmsDevice 类
 * QKmsDevice 中默认使用 primary plane，我们要在 DrmDevice 中同时使用 primary plane 和 overlay plane
 * 
 * 视频数据显示在 overlay plane 上，qt ui 显示在 primary plane 上
 * 并且 ui 要悬浮于视频之上，也就是 primary plane 要在 overlay plane 之上
 *
 * 方案（10.29）
 * DRM 只在本插件操作。camera 4 块与 overlay 3 块是两套池，不 1:1。
 *
 * 1. Qt 创建 3 块屏幕大小的 NV12 overlay framebuffer（CREATE_DUMB + AddFB2），
 *    drmPrimeHandleToFD 导出可写 fd，经 socket 发给 media（带 width/height/pitch）。
 * 2. media 保存这 3 个 fd。采集 DQBUF 后 RGA 旋转缩放到空闲槽的 fd，立刻还 camera。
 * 3. media 只发 {slot, sequence} 通知 Qt：该槽已写好。Qt 用 m_overlayFbs[slot].fb_id
 *    提交 overlay。page flip 完成后再 RELEASE 该槽，media 才能再往里写。
 * 4. 3 槽都忙则丢预览帧，不阻塞 V4L2。断线丢弃旧池，重连再发新的 3 个 fd。
 */
#include "tspi_drmscreen.h"

#include <QLoggingCategory>
#include <QtCore/private/qcore_unix_p.h>
#include <cstring>
#include <drm_fourcc.h>
#include <fcntl.h>
#include <ipc_socket.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcTspiDrm, "qt.qpa.tspi")

DrmDevice::DrmDevice()
    : m_fd(-1)
    , m_res(nullptr)
    , m_planeRes(nullptr)
    , m_conn(nullptr)
    , m_crtcId(0)
    , m_connId(0)
    , m_width(0)
    , m_height(0)
    , m_socket(-1)
{
}

DrmDevice::~DrmDevice()
{
    close();
}

bool DrmDevice::open()
{
    static const char path[] = "/dev/dri/card0";
    int fd = qt_safe_open(path, O_RDWR | O_CLOEXEC);
    if (fd == -1) {
        qErrnoWarning("Could not open DRM device %s", path);
        return false;
    }

    uint64_t hasDumbBuf = 0;
    if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &hasDumbBuf) == -1 || !hasDumbBuf) {
        qWarning("Dumb buffers not supported");
        qt_safe_close(fd);
        return false;
    }

    m_fd = fd;

    qCDebug(lcTspiDrm, "DRM device %s opened", path);

    return true;
}

void DrmDevice::close()
{
    destroyFramebuffers();

    if (m_socket >= 0) {
        qt_safe_close(m_socket);
        m_socket = -1;
    }

    if (m_conn) {
        drmModeFreeConnector(m_conn);
        m_conn = nullptr;
    }
    if (m_planeRes) {
        drmModeFreePlaneResources(m_planeRes);
        m_planeRes = nullptr;
    }
    if (m_res) {
        drmModeFreeResources(m_res);
        m_res = nullptr;
    }

    if (m_fd != -1) {
        qCDebug(lcTspiDrm, "Closing DRM device");
        qt_safe_close(m_fd);
        m_fd = -1;
    }
}

static void getProperty(int fd, drmModeObjectProperties *props,
                        const char *name, uint32_t *propId, uint64_t *value)
{
    for (uint32_t i = 0; i < props->count_props; ++i) {
        drmModePropertyPtr property = drmModeGetProperty(fd, props->props[i]);
        if (property && !strcmp(property->name, name)) {
            if (propId)
                *propId = property->prop_id;
            if (value)
                *value = props->prop_values[i];
            drmModeFreeProperty(property);
            return;
        }
        drmModeFreeProperty(property);
    }
}

bool DrmDevice::getCrtcAndConnector()
{
    for (int i = 0; i < m_res->count_connectors; ++i) {
        drmModeConnectorPtr conn = drmModeGetConnector(m_fd, m_res->connectors[i]);
        uint32_t crtc = 0;

        if (!conn)
            continue;
        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
            for (int e = 0; !crtc && e < conn->count_encoders; ++e) {
                drmModeEncoderPtr enc = drmModeGetEncoder(m_fd, conn->encoders[e]);
                if (!enc)
                    continue;
                for (int c = 0; !crtc && c < m_res->count_crtcs; ++c) {
                    if (enc->possible_crtcs & (1u << c))
                        crtc = m_res->crtcs[c];
                }
                drmModeFreeEncoder(enc);
            }
        }
        if (crtc) {
            m_conn = conn;
            m_connId = conn->connector_id;
            m_crtcId = crtc;
            return true;
        }
        drmModeFreeConnector(conn);
    }
    return false;
}

bool DrmDevice::getPlanes()
{
    int index = -1;
    for (int c = 0; c < m_res->count_crtcs; ++c) {
        if (m_res->crtcs[c] == m_crtcId) {
            index = c;
            break;
        }
    }
    if (index < 0)
        return false;

    const uint32_t mask = 1u << index;
    m_primary.plane_id = 0;
    m_overlay.plane_id = 0;

    for (uint32_t i = 0; i < m_planeRes->count_planes; ++i) {
        uint64_t type = DRM_PLANE_TYPE_CURSOR;
        const uint32_t planeId = m_planeRes->planes[i];
        drmModePlanePtr plane = drmModeGetPlane(m_fd, planeId);
        if (!plane)
            continue;

        drmModeObjectPropertiesPtr props = nullptr;
        if (plane->possible_crtcs & mask)
            props = drmModeObjectGetProperties(m_fd, planeId, DRM_MODE_OBJECT_PLANE);

        if (props) {
            getProperty(m_fd, props, "type", nullptr, &type);
            if (type == DRM_PLANE_TYPE_PRIMARY && m_primary.plane_id == 0)
                m_primary.plane_id = planeId;
            else if (type == DRM_PLANE_TYPE_OVERLAY && m_overlay.plane_id == 0)
                m_overlay.plane_id = planeId;
            drmModeFreeObjectProperties(props);
        }
        drmModeFreePlane(plane);
    }

    if (m_primary.plane_id == 0 || m_overlay.plane_id == 0) {
        qWarning("Cannot find primary or overlay plane");
        return false;
    }

    return true;
}

bool DrmDevice::loadPlaneProp(uint32_t planeId, PlaneProp *prop)
{
    *prop = PlaneProp();
    drmModeObjectPropertiesPtr props =
        drmModeObjectGetProperties(m_fd, planeId, DRM_MODE_OBJECT_PLANE);
    if (!props)
        return false;

    prop->plane_id = planeId;
    getProperty(m_fd, props, "CRTC_ID", &prop->crtc_id, nullptr);
    getProperty(m_fd, props, "FB_ID", &prop->fb_id, nullptr);
    getProperty(m_fd, props, "CRTC_X", &prop->crtc_x, nullptr);
    getProperty(m_fd, props, "CRTC_Y", &prop->crtc_y, nullptr);
    getProperty(m_fd, props, "CRTC_W", &prop->crtc_w, nullptr);
    getProperty(m_fd, props, "CRTC_H", &prop->crtc_h, nullptr);
    getProperty(m_fd, props, "SRC_X", &prop->src_x, nullptr);
    getProperty(m_fd, props, "SRC_Y", &prop->src_y, nullptr);
    getProperty(m_fd, props, "SRC_W", &prop->src_w, nullptr);
    getProperty(m_fd, props, "SRC_H", &prop->src_h, nullptr);
    getProperty(m_fd, props, "zpos", &prop->zpos, nullptr);
    drmModeFreeObjectProperties(props);

    if (!prop->crtc_id || !prop->fb_id
        || !prop->crtc_x || !prop->crtc_y || !prop->crtc_w || !prop->crtc_h
        || !prop->src_x || !prop->src_y || !prop->src_w || !prop->src_h) {
        qWarning("Plane %u: missing properties", planeId);
        return false;
    }
    return true;
}

bool DrmDevice::createScreens()
{
    if (drmSetClientCap(m_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) != 0 ||
        drmSetClientCap(m_fd, DRM_CLIENT_CAP_ATOMIC, 1) != 0) {
        qErrnoWarning("drmSetClientCap failed");
        return false;
    }

    m_res = drmModeGetResources(m_fd);
    if (!m_res) {
        qErrnoWarning(errno, "drmModeGetResources failed");
        return false;
    }

    m_planeRes = drmModeGetPlaneResources(m_fd);
    if (!m_planeRes) {
        qErrnoWarning("drmModeGetPlaneResources failed");
        return false;
    }

    if (!getCrtcAndConnector()) {
        qWarning("Cannot get crtc and connector");
        return false;
    }

    if (!getPlanes())
        return false;

    if (!loadPlaneProp(m_primary.plane_id, &m_primary)
        || !loadPlaneProp(m_overlay.plane_id, &m_overlay)) {
        qWarning("Cannot load plane properties");
        return false;
    }

    m_width = m_conn->modes[0].hdisplay;
    m_height = m_conn->modes[0].vdisplay;

    qCDebug(lcTspiDrm, "display: width %u height %u conn_id %u crtc_id %u",
            m_width, m_height, m_connId, m_crtcId);
    qCDebug(lcTspiDrm, "display: primary_id %u overlay_id %u",
            m_primary.plane_id, m_overlay.plane_id);

    uint32_t propConnCrtcId = 0;
    uint32_t propCrtcActive = 0;
    uint32_t propCrtcModeId = 0;
    uint32_t blobId = 0;

    drmModeObjectPropertiesPtr props =
        drmModeObjectGetProperties(m_fd, m_connId, DRM_MODE_OBJECT_CONNECTOR);
    if (!props) {
        qWarning("Cannot get connector properties");
        return false;
    }
    getProperty(m_fd, props, "CRTC_ID", &propConnCrtcId, nullptr);
    drmModeFreeObjectProperties(props);
    if (propConnCrtcId == 0) {
        qWarning("Cannot find connector property CRTC_ID");
        return false;
    }

    props = drmModeObjectGetProperties(m_fd, m_crtcId, DRM_MODE_OBJECT_CRTC);
    if (!props) {
        qWarning("Cannot get crtc properties");
        return false;
    }
    getProperty(m_fd, props, "ACTIVE", &propCrtcActive, nullptr);
    getProperty(m_fd, props, "MODE_ID", &propCrtcModeId, nullptr);
    drmModeFreeObjectProperties(props);
    if (propCrtcActive == 0 || propCrtcModeId == 0) {
        qWarning("Cannot find crtc property ACTIVE or MODE_ID");
        return false;
    }

    if (drmModeCreatePropertyBlob(m_fd, &m_conn->modes[0],
                                  sizeof(m_conn->modes[0]), &blobId) != 0) {
        qErrnoWarning("drmModeCreatePropertyBlob failed");
        return false;
    }

    drmModeAtomicReq *req = drmModeAtomicAlloc();
    if (!req) {
        drmModeDestroyPropertyBlob(m_fd, blobId);
        return false;
    }

    drmModeAtomicAddProperty(req, m_crtcId, propCrtcActive, 1);
    drmModeAtomicAddProperty(req, m_crtcId, propCrtcModeId, blobId);
    drmModeAtomicAddProperty(req, m_connId, propConnCrtcId, m_crtcId);
    const int ret = drmModeAtomicCommit(m_fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, nullptr);
    drmModeAtomicFree(req);
    drmModeDestroyPropertyBlob(m_fd, blobId);
    if (ret != 0) {
        qErrnoWarning("modeset atomic commit failed");
        return false;
    }

    return true;
}

bool DrmDevice::createFramebuffer(Framebuffer *buf, uint32_t format, bool map, bool export_fd)
{
    struct drm_mode_create_dumb create = {};
    uint32_t handles[4] = {};
    uint32_t pitches[4] = {};
    uint32_t offsets[4] = {};

    if (!buf || m_width == 0 || m_height == 0)
        return false;

    *buf = Framebuffer();

    create.width = m_width;
    switch (format) {
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_ARGB8888:
        create.height = m_height;
        create.bpp = 32;
        break;
    case DRM_FORMAT_NV12:
        create.height = m_height * 3 / 2;
        create.bpp = 8;
        break;
    default:
        qWarning("createFramebuffer: unsupported format 0x%08x", format);
        return false;
    }

    if (drmIoctl(m_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
        qErrnoWarning("CREATE_DUMB failed");
        return false;
    }

    buf->width = m_width;
    buf->height = m_height;
    buf->format = format;
    buf->pitch = create.pitch;
    buf->handle = create.handle;
    buf->size = create.size;

    handles[0] = create.handle;
    pitches[0] = create.pitch;
    if (format == DRM_FORMAT_NV12) {
        handles[1] = create.handle;
        pitches[1] = create.pitch;
        offsets[1] = create.pitch * m_height;
        if (create.pitch < m_width || create.size < offsets[1] + offsets[1] / 2) {
            qWarning("createFramebuffer: NV12 buffer too small, pitch %u size %llu",
                     create.pitch, static_cast<unsigned long long>(create.size));
            destroyFramebuffer(buf);
            return false;
        }
    }

    if (drmModeAddFB2(m_fd, m_width, m_height, format,
                      handles, pitches, offsets, &buf->fb_id, 0) != 0) {
        qErrnoWarning("drmModeAddFB2 %ux%u failed", m_width, m_height);
        destroyFramebuffer(buf);
        return false;
    }

    if (map) {
        struct drm_mode_map_dumb mreq = {};
        mreq.handle = create.handle;
        if (drmIoctl(m_fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq) < 0) {
            qErrnoWarning("MAP_DUMB failed");
            destroyFramebuffer(buf);
            return false;
        }
        buf->vaddr = mmap(nullptr, create.size, PROT_READ | PROT_WRITE,
                          MAP_SHARED, m_fd, mreq.offset);
        if (buf->vaddr == MAP_FAILED) {
            qErrnoWarning("mmap dumb failed");
            buf->vaddr = nullptr;
            destroyFramebuffer(buf);
            return false;
        }
        if (format == DRM_FORMAT_XRGB8888 || format == DRM_FORMAT_ARGB8888) {
            memset(buf->vaddr, 0, create.size);
            buf->wrapper = QImage(static_cast<uchar *>(buf->vaddr),
                                  int(buf->width), int(buf->height),
                                  int(buf->pitch),
                                  format == DRM_FORMAT_ARGB8888
                                      ? QImage::Format_ARGB32
                                      : QImage::Format_RGB32);
        }
    }

    if (export_fd) {
        if (drmPrimeHandleToFD(m_fd, create.handle, DRM_CLOEXEC | DRM_RDWR,
                               &buf->prime_fd) != 0) {
            qErrnoWarning("drmPrimeHandleToFD failed");
            buf->prime_fd = -1;
            destroyFramebuffer(buf);
            return false;
        }
    }

    qCDebug(lcTspiDrm, "dumb buffer %ux%u format 0x%x handle %u pitch %u size %llu fb %u",
            buf->width, buf->height, buf->format, buf->handle, buf->pitch,
            static_cast<unsigned long long>(buf->size), buf->fb_id);
    return true;
}

void DrmDevice::destroyFramebuffer(Framebuffer *buf)
{
    if (!buf)
        return;

    buf->wrapper = QImage();
    if (buf->prime_fd >= 0)
        qt_safe_close(buf->prime_fd);
    if (buf->vaddr)
        munmap(buf->vaddr, buf->size);
    if (buf->fb_id)
        drmModeRmFB(m_fd, buf->fb_id);
    if (buf->handle) {
        if (buf->imported) {
            struct drm_gem_close closeArg = {};
            closeArg.handle = buf->handle;
            drmIoctl(m_fd, DRM_IOCTL_GEM_CLOSE, &closeArg);
        } else {
            struct drm_mode_destroy_dumb destroy = {};
            destroy.handle = buf->handle;
            drmIoctl(m_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
        }
    }
    *buf = Framebuffer();
}

void DrmDevice::destroyFramebuffers()
{
    for (int i = 0; i < PRIMARY_BUFFER_COUNT; ++i)
        destroyFramebuffer(&m_primary_fbs[i]);
    for (int i = 0; i < OVERLAY_BUFFER_COUNT; ++i)
        destroyFramebuffer(&m_overlay_fbs[i]);
}

void DrmDevice::createFramebuffers()
{
    // 为 UI 准备 framebuffer
    for (int i = 0; i < PRIMARY_BUFFER_COUNT; ++i) {
        if (!createFramebuffer(&m_primary_fbs[i], DRM_FORMAT_XRGB8888, true, false))
            return;
    }
    // 为 video 准备 framebuffer
    for (int i = 0; i < OVERLAY_BUFFER_COUNT; ++i) {
        if (!createFramebuffer(&m_overlay_fbs[i], DRM_FORMAT_NV12, false, true))
            return;
    }
}

TspiDrmScreen::TspiDrmScreen(const QStringList &args)
    : m_device(nullptr)
{
    Q_UNUSED(args);
}

TspiDrmScreen::~TspiDrmScreen()
{
    if (m_device) {
        m_device->close();
        delete m_device;
    }
}

// 初始化屏幕
bool TspiDrmScreen::initialize()
{
    m_device = new DrmDevice;
    if (!m_device->open())
        return false;
    if (!m_device->createScreens())
        return false;
    // 为 ui 创建 framebuffer
    m_device->createFramebuffers();

    if (!m_device->createSocket()) {
        qWarning("preview: send overlay fds failed");
    }

    return QFbScreen::initialize();
}

bool DrmDevice::createSocket()
{
    int fds[OVERLAY_BUFFER_COUNT];
    tspi_preview_buf msg = {};

    m_socket = ipc_client_create(TSPI_PREVIEW_SOCKET_PATH);
    if (m_socket < 0) {
        qErrnoWarning("preview socket connect %s failed", TSPI_PREVIEW_SOCKET_PATH);
        return false;
    }

    msg.width = m_overlay_fbs[0].width;
    msg.height = m_overlay_fbs[0].height;
    msg.pitch = m_overlay_fbs[0].pitch;
    msg.count = OVERLAY_BUFFER_COUNT;
    for (int i = 0; i < OVERLAY_BUFFER_COUNT; ++i)
        fds[i] = m_overlay_fbs[i].prime_fd;

    if (ipc_send(m_socket, &msg, sizeof(msg), fds, OVERLAY_BUFFER_COUNT) < 0) {
        qErrnoWarning("preview send overlay fds failed");
        return false;
    }
    return true;
}

// 处理屏幕重绘
QRegion TspiDrmScreen::doRedraw()
{
    return QFbScreen::doRedraw();
}

// 获取屏幕或窗口的截图
QPixmap TspiDrmScreen::grabWindow(WId wid, int x, int y, int width, int height) const
{
    Q_UNUSED(wid);
    Q_UNUSED(x);
    Q_UNUSED(y);
    Q_UNUSED(width);
    Q_UNUSED(height);

    return QPixmap();
}

QT_END_NAMESPACE
