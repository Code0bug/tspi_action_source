/*
 * 参考 qt5base/src/plugins/platforms/linuxfb/qlinuxfbdrmscreen.cpp 实现
 * qlinuxfbdrmscreen 中定义 QLinuxFbDevice 类，其继承 QKmsDevice 类
 * QLinuxFbDevice 类本质是对 DRM 设备的管理，QKmsDevice 是基类封装
 *
 * tspi_drmscreen 中定义 DrmDevice 类，替代 QLinuxFbDevice + QKmsDevice 类
 * QKmsDevice 中默认使用 primary plane，我们要在 DrmDevice 中同时使用 primary plane 和 overlay plane
 * 
 * 同时，需要支持 socket 接收 media 进程发来的视频数据的 dma-buf fd
 * 视频数据显示在 overlay plane 上，qt ui 显示在 primary plane 上
 * 并且 ui 要悬浮于视频之上，也就是 primary plane 要在 overlay plane 之上
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

// 为 UI 准备 framebuffer
bool DrmDevice::createFramebuffer(Framebuffer *buf)
{
    struct drm_mode_create_dumb create = {};
    uint32_t handles[4] = {};
    uint32_t pitches[4] = {};
    uint32_t offsets[4] = {};

    create.width = m_width;
    create.height = m_height;
    create.bpp = 32;
    if (drmIoctl(m_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
        qErrnoWarning("CREATE_DUMB failed");
        return false;
    }

    buf->width = m_width;
    buf->height = m_height;
    buf->format = DRM_FORMAT_XRGB8888;
    buf->pitch = create.pitch;
    buf->handle = create.handle;
    buf->size = create.size;

    handles[0] = create.handle;
    pitches[0] = create.pitch;
    if (drmModeAddFB2(m_fd, m_width, m_height, DRM_FORMAT_XRGB8888,
                      handles, pitches, offsets, &buf->fb_id, 0) != 0) {
        qErrnoWarning("drmModeAddFB2 %ux%u failed", m_width, m_height);
        destroyFramebuffer(buf);
        return false;
    }

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
    memset(buf->vaddr, 0, create.size);
    buf->wrapper = QImage(static_cast<uchar *>(buf->vaddr),
                          int(buf->width), int(buf->height),
                          int(buf->pitch), QImage::Format_RGB32);

    qCDebug(lcTspiDrm, "dumb buffer %ux%u format 0x%x handle %u pitch %u size %llu fb %u",
            buf->width, buf->height, buf->format, buf->handle, buf->pitch,
            static_cast<unsigned long long>(buf->size), buf->fb_id);
    return true;
}

// 为视频数据准备 framebuffer
bool DrmDevice::importFramebuffer(Framebuffer *buf, int primeFd)
{
    uint32_t handles[4] = {};
    uint32_t pitches[4] = {};
    uint32_t offsets[4] = {};

    if (drmPrimeFDToHandle(m_fd, primeFd, &buf->handle) != 0) {
        qErrnoWarning("drmPrimeFDToHandle failed");
        return false;
    }

    buf->imported = true;
    buf->format = DRM_FORMAT_NV12;
    handles[0] = buf->handle;
    handles[1] = buf->handle;
    pitches[0] = buf->pitch;
    pitches[1] = buf->pitch;
    offsets[1] = buf->pitch * buf->height;
    buf->size = offsets[1] + offsets[1] / 2;

    if (drmModeAddFB2(m_fd, buf->width, buf->height, DRM_FORMAT_NV12,
                      handles, pitches, offsets, &buf->fb_id, 0) != 0) {
        qErrnoWarning("drmModeAddFB2 import %ux%u failed", buf->width, buf->height);
        destroyFramebuffer(buf);
        return false;
    }

    qCDebug(lcTspiDrm, "imported buffer %ux%u format 0x%x handle %u pitch %u fb %u",
            buf->width, buf->height, buf->format, buf->handle, buf->pitch, buf->fb_id);
    return true;
}

bool DrmDevice::importFramebuffers(const int *fds, int count,
                                   uint32_t width, uint32_t height, uint32_t pitch)
{
    if (!fds || count <= 0)
        return false;

    m_overlayFbs.resize(count);
    for (int i = 0; i < count; ++i) {
        m_overlayFbs[i].width = width;
        m_overlayFbs[i].height = height;
        m_overlayFbs[i].pitch = pitch;
        if (!importFramebuffer(&m_overlayFbs[i], fds[i])) {
            for (Framebuffer &buf : m_overlayFbs)
                destroyFramebuffer(&buf);
            m_overlayFbs.clear();
            return false;
        }
    }
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
    for (int i = 0; i < BUFFER_COUNT; ++i)
        destroyFramebuffer(&m_fbs[i]);
    for (Framebuffer &buf : m_overlayFbs)
        destroyFramebuffer(&buf);
    m_overlayFbs.clear();
}

void DrmDevice::createFramebuffers()
{
    for (int i = 0; i < BUFFER_COUNT; ++i) {
        if (!createFramebuffer(&m_fbs[i]))
            return;
    }
}

TspiDrmScreen::TspiDrmScreen(const QStringList &args)
    : m_device(nullptr)
    , m_previewSocket(-1)
{
    Q_UNUSED(args);
}

TspiDrmScreen::~TspiDrmScreen()
{
    if (m_previewSocket >= 0) {
        qt_safe_close(m_previewSocket);
        m_previewSocket = -1;
    }
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

    // 创建 socket 客户端
    m_previewSocket = ipc_client_create(TSPI_PREVIEW_SOCKET_PATH);
    if (m_previewSocket < 0) {
        qErrnoWarning("preview socket connect %s failed", TSPI_PREVIEW_SOCKET_PATH);
        return false;
    }
    // 导入 camera dma-buf fd 到 drm framebuffer 
    if (!receivePreviewBuffers()) {
        qWarning("preview: connected, but receiving camera fds failed");
    }

    return QFbScreen::initialize();
}

bool TspiDrmScreen::receivePreviewBuffers()
{
    int flags = fcntl(m_previewSocket, F_GETFL);
    if (flags >= 0)
        fcntl(m_previewSocket, F_SETFL, flags & ~O_NONBLOCK);

    tspi_preview_buf msg = {};
    int fd = -1;
    if (ipc_rec(m_previewSocket, &msg, sizeof(msg), &fd) != ssize_t(sizeof(msg))
        || fd < 0 || msg.count == 0 || msg.slot >= msg.count) {
        qErrnoWarning("preview recv first camera fd failed");
        if (fd >= 0)
            qt_safe_close(fd);
        return false;
    }

    QVector<int> fds(int(msg.count), -1);
    fds[int(msg.slot)] = fd;
    qWarning("preview: recv slot=%u fd=%d %ux%u pitch=%u count=%u",
             msg.slot, fd, msg.width, msg.height, msg.pitch, msg.count);
    for (uint32_t i = 1; i < msg.count; ++i) {
        tspi_preview_buf next = {};
        fd = -1;
        if (ipc_rec(m_previewSocket, &next, sizeof(next), &fd) != ssize_t(sizeof(next))
            || fd < 0 || next.slot >= msg.count || fds[int(next.slot)] >= 0) {
            qErrnoWarning("preview recv camera fd %u failed", i);
            if (fd >= 0)
                qt_safe_close(fd);
            for (int owned : fds) {
                if (owned >= 0)
                    qt_safe_close(owned);
            }
            return false;
        }
        fds[int(next.slot)] = fd;
        qWarning("preview: recv slot=%u fd=%d", next.slot, fd);
    }

    const bool ok = m_device->importFramebuffers(fds.constData(), int(msg.count),
                                                 msg.width, msg.height, msg.pitch);
    for (int owned : fds) {
        if (owned >= 0)
            qt_safe_close(owned);
    }
    if (!ok)
        qWarning("preview: importFramebuffers failed");
    return ok;
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
