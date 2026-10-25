#include <future>
#include "display_thread.h"
#include <preview_protocol.h>
#include <preview_queue.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <array>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {
struct Property {
    uint32_t id = 0;
    uint64_t value = 0, minimum = 0, maximum = 0;
    bool immutable = false;
    std::map<std::string, uint64_t> enums;
};
using Properties = std::map<std::string, Property>;

Properties properties(int fd, uint32_t object, uint32_t type)
{
    Properties result;
    drmModeObjectProperties *list = drmModeObjectGetProperties(fd, object, type);
    if (!list) return result;
    for (uint32_t i = 0; i < list->count_props; ++i) {
        drmModePropertyPtr p = drmModeGetProperty(fd, list->props[i]);
        if (!p) continue;
        Property value;
        value.id = p->prop_id;
        value.value = list->prop_values[i];
        value.immutable = p->flags & DRM_MODE_PROP_IMMUTABLE;
        value.minimum = value.maximum = value.value;
        if ((p->flags & DRM_MODE_PROP_RANGE) && p->count_values >= 2 && !value.immutable) {
            value.minimum = p->values[0]; value.maximum = p->values[1];
        }
        for (int j = 0; j < p->count_enums; ++j) value.enums[p->enums[j].name] = p->enums[j].value;
        result[p->name] = value;
        drmModeFreeProperty(p);
    }
    drmModeFreeObjectProperties(list);
    return result;
}

struct Buffer {
    uint32_t handle = 0, fb = 0, pitch = 0;
    uint64_t size = 0;
    unsigned char *map = nullptr;
    void release(int fd, bool imported = false) {
        if (map) munmap(map, size);
        if (fd >= 0 && fb) drmModeRmFB(fd, fb);
        if (fd >= 0 && handle) {
            if (imported) { drm_gem_close arg{}; arg.handle = handle; drmIoctl(fd, DRM_IOCTL_GEM_CLOSE, &arg); }
            else { drm_mode_destroy_dumb arg{}; arg.handle = handle; drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &arg); }
        }
        *this = Buffer{};
    }
    bool create(int fd, uint32_t w, uint32_t h, bool nv12 = false) {
        drm_mode_create_dumb arg{};
        arg.width = w; arg.height = nv12 ? h * 3 / 2 : h; arg.bpp = nv12 ? 8 : 32;
        if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &arg)) return false;
        handle = arg.handle; pitch = arg.pitch; size = arg.size;
        uint32_t handles[4] = {handle, nv12 ? handle : 0, 0, 0};
        uint32_t pitches[4] = {pitch, nv12 ? pitch : 0, 0, 0};
        uint32_t offsets[4] = {0, nv12 ? pitch * h : 0, 0, 0};
        if (drmModeAddFB2(fd, w, h, nv12 ? DRM_FORMAT_NV12 : DRM_FORMAT_ARGB8888,
                           handles, pitches, offsets, &fb, 0)) return false;
        if (nv12) return true;
        drm_mode_map_dumb mapping{}; mapping.handle = handle;
        if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mapping)) return false;
        void *address = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mapping.offset);
        if (address == MAP_FAILED) return false;
        map = static_cast<unsigned char *>(address);
        std::memset(map, 0, size);
        return true;
    }
};

struct Plane {
    uint32_t id = 0;
    Properties props;
    bool argb = false, nv12 = false;
    uint64_t z = 0;
};

struct Display {
    int fd = -1, epoll = -1, socket = -1;
    uint32_t connector = 0, crtc = 0, blob = 0;
    uint32_t width = 0, height = 0;
    Properties connectorProps, crtcProps;
    Plane ui, video;
    std::vector<Plane> planes;
    std::array<Buffer, 3> uiBuffers, videoBuffers;
    std::array<QRegion, 3> damage;
    PreviewQueue queue;
    preview_packet pool{};
    bool pending = false, retiring = false, modeset = true, uiDirty = false;
    std::chrono::steady_clock::time_point submittedAt{};
    int uiCurrent = 0, uiSubmitted = 0;
    std::vector<preview_packet> returns;
    QImage image;
    std::chrono::steady_clock::time_point reconnect{};

    bool add(drmModeAtomicReq *req, uint32_t object, Properties &props,
             const char *name, uint64_t value, bool optional = false) {
        auto p = props.find(name);
        if (p == props.end()) return optional;
        if (p->second.immutable) return p->second.value == value;
        return drmModeAtomicAddProperty(req, object, p->second.id, value) >= 0;
    }
    bool plane(drmModeAtomicReq *req, Plane &p, uint32_t fb) {
        bool ok = add(req, p.id, p.props, "FB_ID", fb) &&
                  add(req, p.id, p.props, "CRTC_ID", fb ? crtc : 0);
        if (!fb) return ok;
        ok = ok && add(req, p.id, p.props, "CRTC_X", 0) && add(req, p.id, p.props, "CRTC_Y", 0) &&
            add(req, p.id, p.props, "CRTC_W", width) && add(req, p.id, p.props, "CRTC_H", height) &&
            add(req, p.id, p.props, "SRC_X", 0) && add(req, p.id, p.props, "SRC_Y", 0) &&
            add(req, p.id, p.props, "SRC_W", uint64_t(width) << 16) &&
            add(req, p.id, p.props, "SRC_H", uint64_t(height) << 16) &&
            add(req, p.id, p.props, "zpos", p.z);
        auto alpha = p.props.find("alpha");
        if (alpha != p.props.end()) ok = ok && add(req, p.id, p.props, "alpha", alpha->second.maximum);
        auto blend = p.props.find("pixel blend mode");
        if (p.id == ui.id && blend != p.props.end()) {
            auto premultiplied = blend->second.enums.find("Pre-multiplied");
            if (premultiplied == blend->second.enums.end()) return false;
            ok = ok && add(req, p.id, p.props, "pixel blend mode", premultiplied->second);
        }
        return ok;
    }
    drmModeAtomicReq *request(uint32_t uiFb, uint32_t videoFb) {
        drmModeAtomicReq *req = drmModeAtomicAlloc();
        if (!req) return nullptr;
        bool ok = plane(req, ui, uiFb) && plane(req, video, videoFb);
        if (modeset) {
            ok = ok && add(req, connector, connectorProps, "CRTC_ID", crtc) &&
                add(req, crtc, crtcProps, "ACTIVE", 1) && add(req, crtc, crtcProps, "MODE_ID", blob);
            for (Plane &p : planes) if (p.id != ui.id && p.id != video.id && p.props["CRTC_ID"].value == crtc)
                ok = ok && plane(req, p, 0);
        }
        if (!ok) { drmModeAtomicFree(req); return nullptr; }
        return req;
    }

    bool initialize();
    bool submit();
    bool import(const preview_packet &description, const int *fds);
    void connectPreview();
    void receive();
    void flushReturns();
    void release(int slot);
    void disconnected();
    void completed();
    void clearPool();
    void copyUi(int slot);
    ~Display();
};

bool Display::initialize()
{
    const char *device = std::getenv("TSPI_DRM_DEVICE");
    fd = open(device && *device ? device : "/dev/dri/card0", O_RDWR | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0 || drmSetMaster(fd) || drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) ||
        drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1)) return false;
    drmModeRes *res = drmModeGetResources(fd);
    if (!res) return false;
    drmModeModeInfo mode{};
    uint32_t mask = 0;
    for (int i = 0; !connector && i < res->count_connectors; ++i) {
        drmModeConnector *c = drmModeGetConnector(fd, res->connectors[i]);
        if (!c) continue;
        if (c->connection == DRM_MODE_CONNECTED && c->count_modes) {
            for (int e = 0; !connector && e < c->count_encoders; ++e) {
                drmModeEncoder *encoder = drmModeGetEncoder(fd, c->encoders[e]);
                if (!encoder) continue;
                for (int j = 0; j < res->count_crtcs; ++j) if (encoder->possible_crtcs & (1u << j)) {
                    crtc = res->crtcs[j]; mask = 1u << j; connector = c->connector_id;
                    mode = c->modes[0];
                    for (int m = 0; m < c->count_modes; ++m)
                        if (c->modes[m].type & DRM_MODE_TYPE_PREFERRED) { mode = c->modes[m]; break; }
                    break;
                }
                drmModeFreeEncoder(encoder);
            }
        }
        drmModeFreeConnector(c);
    }
    drmModeFreeResources(res);
    if (!connector || (mode.hdisplay & 1) || (mode.vdisplay & 1)) return false;
    width = mode.hdisplay; height = mode.vdisplay;
    connectorProps = properties(fd, connector, DRM_MODE_OBJECT_CONNECTOR);
    crtcProps = properties(fd, crtc, DRM_MODE_OBJECT_CRTC);
    if (drmModeCreatePropertyBlob(fd, &mode, sizeof(mode), &blob)) return false;
    drmModePlaneRes *list = drmModeGetPlaneResources(fd);
    if (!list) return false;
    for (uint32_t i = 0; i < list->count_planes; ++i) {
        drmModePlane *p = drmModeGetPlane(fd, list->planes[i]);
        if (!p) continue;
        if (p->possible_crtcs & mask) {
            Plane candidate;
            candidate.id = p->plane_id;
            candidate.props = properties(fd, p->plane_id, DRM_MODE_OBJECT_PLANE);
            for (uint32_t f = 0; f < p->count_formats; ++f) {
                candidate.argb |= p->formats[f] == DRM_FORMAT_ARGB8888;
                candidate.nv12 |= p->formats[f] == DRM_FORMAT_NV12;
            }
            if (candidate.props["type"].value != DRM_PLANE_TYPE_CURSOR) planes.push_back(candidate);
        }
        drmModeFreePlane(p);
    }
    drmModeFreePlaneResources(list);
    for (Buffer &b : uiBuffers) if (!b.create(fd, width, height)) return false;
    Buffer test;
    if (!test.create(fd, width, height, true)) { test.release(fd); return false; }
    bool found = false;
    for (const Plane &v : planes) {
        if (!v.nv12 || !v.props.count("zpos")) continue;
        for (const Plane &u : planes) {
            if (!u.argb || u.id == v.id || !u.props.count("zpos")) continue;
            video = v; ui = u;
            video.z = v.props.at("zpos").minimum;
            ui.z = std::max(u.props.at("zpos").minimum, video.z + 1);
            if (ui.z > u.props.at("zpos").maximum) continue;
            drmModeAtomicReq *req = request(uiBuffers[0].fb, test.fb);
            if (req) {
                found = drmModeAtomicCommit(fd, req, DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_ALLOW_MODESET, nullptr) == 0;
                drmModeAtomicFree(req);
            }
            if (found) {
                req = request(uiBuffers[0].fb, 0);
                found = req && drmModeAtomicCommit(fd, req,
                    DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_ALLOW_MODESET, nullptr) == 0;
                if (req) drmModeAtomicFree(req);
            }
            if (found) break;
        }
        if (found) break;
    }
    test.release(fd);
    if (!found) { std::fprintf(stderr, "tspi: no atomic-compatible NV12 + ARGB plane pair\n"); return false; }
    drmModeAtomicReq *req = request(uiBuffers[0].fb, 0);
    if (!req) return false;
    int result = drmModeAtomicCommit(fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, nullptr);
    drmModeAtomicFree(req);
    if (result) return false;
    modeset = false;
    epoll = epoll_create1(EPOLL_CLOEXEC);
    if (epoll < 0) return false;
    epoll_event event{}; event.events = EPOLLIN; event.data.fd = fd;
    if (epoll_ctl(epoll, EPOLL_CTL_ADD, fd, &event)) return false;
    image = QImage(height, width, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    for (QRegion &r : damage) r = QRect(0, 0, height, width);
    std::fprintf(stderr, "tspi: %ux%u crtc=%u video=%u z=%llu ui=%u z=%llu\n", width, height, crtc,
        video.id, static_cast<unsigned long long>(video.z), ui.id, static_cast<unsigned long long>(ui.z));
    return true;
}

void Display::copyUi(int slot)
{
    Buffer &b = uiBuffers[slot];
    /* Rotate logical landscape UI clockwise into portrait scanout. Read only
     * cached QImage RAM; dumb mappings receive writes only. Damage is tracked
     * independently for each buffer, including skipped GUI generations. */
    for (const QRect &rect : damage[slot]) {
        QRect clipped = rect.intersected(image.rect());
        for (int x = clipped.left(); x <= clipped.right(); ++x) {
            auto *dst = reinterpret_cast<QRgb *>(b.map + uint64_t(x) * b.pitch);
            for (int y = clipped.bottom(); y >= clipped.top(); --y) {
                const QRgb *src = reinterpret_cast<const QRgb *>(image.constScanLine(y));
                dst[width - 1 - y] = src[x];
            }
        }
    }
    damage[slot] = QRegion();
}

bool Display::submit()
{
    if (pending) {
        if (std::chrono::steady_clock::now() - submittedAt > std::chrono::seconds(3)) {
            std::fprintf(stderr, "tspi: timed out waiting for DRM page flip\n");
            return false;
        }
        return true;
    }
    if (retiring && queue.current() < 0) { clearPool(); return true; }
    if (!uiDirty && queue.waiting() < 0 && !retiring) return true;
    int nextUi = uiCurrent;
    if (uiDirty) {
        nextUi = (uiCurrent + 1) % 3;
        copyUi(nextUi);
    }
    int nextVideo = retiring ? -1 : queue.next();
    drmModeAtomicReq *req = request(uiBuffers[nextUi].fb, nextVideo < 0 ? 0 : videoBuffers[nextVideo].fb);
    if (!req) return false;
    int result = drmModeAtomicCommit(fd, req, DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT, this);
    drmModeAtomicFree(req);
    if (result) return false;
    pending = true;
    submittedAt = std::chrono::steady_clock::now();
    uiSubmitted = nextUi;
    uiDirty = false;
    queue.submitted(nextVideo);
    return true;
}

void Display::release(int slot)
{
    if (socket < 0 || slot < 0) return;
    preview_packet packet = preview_message(PREVIEW_RELEASE);
    packet.generation = pool.generation;
    packet.slot = slot;
    packet.sequence = queue.sequence(slot);
    returns.push_back(packet);
}

void Display::completed()
{
    if (!pending) return;
    int slot = queue.complete();
    pending = false;
    uiCurrent = uiSubmitted;
    release(slot);
    if (retiring && queue.current() < 0) clearPool();
}

void Display::clearPool()
{
    for (Buffer &b : videoBuffers) b.release(fd, true);
    pool = preview_packet{};
    queue = PreviewQueue{};
    retiring = false;
}

void Display::disconnected()
{
    if (socket >= 0) { epoll_ctl(epoll, EPOLL_CTL_DEL, socket, nullptr); close(socket); }
    socket = -1;
    returns.clear();
    queue.discardWaiting();
    retiring = pool.buffer_count != 0;
    reconnect = std::chrono::steady_clock::now() + std::chrono::seconds(1);
}

void Display::connectPreview()
{
    if (socket >= 0 || retiring || std::chrono::steady_clock::now() < reconnect) return;
    sockaddr_un address{};
    const char *path = preview_socket_path();
    if (std::strlen(path) >= sizeof(address.sun_path)) return;
    address.sun_family = AF_UNIX;
    std::strcpy(address.sun_path, path);
    socket = ::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (socket < 0) { disconnected(); return; }
    if (::connect(socket, reinterpret_cast<sockaddr *>(&address), sizeof(address))) { disconnected(); return; }
    preview_packet hello = preview_message(PREVIEW_HELLO);
    hello.width = width; hello.height = height;
    if (preview_send(socket, &hello, nullptr) != 1) { disconnected(); return; }
    epoll_event event{}; event.events = EPOLLIN | EPOLLRDHUP; event.data.fd = socket;
    if (epoll_ctl(epoll, EPOLL_CTL_ADD, socket, &event)) disconnected();
}

bool Display::import(const preview_packet &p, const int *fds)
{
    if (pool.buffer_count || p.width != width || p.height != height || p.format != DRM_FORMAT_NV12 ||
        p.pitch < width || (p.pitch & 15) || p.pitch > 16384 || p.buffer_count != 3 ||
        uint64_t(p.buffer_bytes) < uint64_t(p.pitch) * height * 3 / 2 || !p.generation) return false;
    /* Set pool first so partial imports are also retired on failure. */
    pool = p;
    for (int i = 0; i < 3; ++i) {
        Buffer &b = videoBuffers[i];
        off_t length = lseek(fds[i], 0, SEEK_END);
        if (length < 0 || uint64_t(length) < p.buffer_bytes || drmPrimeFDToHandle(fd, fds[i], &b.handle)) return false;
        for (int j = 0; j < i; ++j) if (videoBuffers[j].handle == b.handle) {
            b.handle = 0; // PRIME re-import returns the same handle without another handle reference.
            return false;
        }
        uint32_t handles[4] = {b.handle, b.handle, 0, 0};
        uint32_t pitches[4] = {p.pitch, p.pitch, 0, 0};
        uint32_t offsets[4] = {0, p.pitch * height, 0, 0};
        if (drmModeAddFB2(fd, width, height, p.format, handles, pitches, offsets, &b.fb, 0)) return false;
    }
    std::fprintf(stderr, "tspi: imported preview pool %llu\n", static_cast<unsigned long long>(p.generation));
    return true;
}

void Display::receive()
{
    for (int n = 0; socket >= 0 && n < 32; ++n) {
        preview_packet packet{};
        int fds[3];
        int result = preview_recv(socket, &packet, fds);
        if (!result) break;
        if (result < 0) { disconnected(); break; }
        bool valid = false;
        if (packet.type == PREVIEW_INIT) valid = import(packet, fds);
        else if (packet.type == PREVIEW_FRAME && pool.buffer_count && packet.generation == pool.generation) {
            int dropped;
            valid = queue.offer(packet.slot, packet.sequence, dropped);
            if (valid) release(dropped);
        }
        for (int value : fds) if (value >= 0) close(value);
        if (!valid) { std::fprintf(stderr, "tspi: invalid preview packet/import\n"); disconnected(); break; }
    }
}

void Display::flushReturns()
{
    while (socket >= 0 && !returns.empty()) {
        int result = preview_send(socket, &returns.front(), nullptr);
        if (result < 0) { disconnected(); break; }
        if (!result) break;
        returns.erase(returns.begin());
    }
    if (socket >= 0) {
        epoll_event event{};
        event.events = EPOLLIN | EPOLLRDHUP;
        if (!returns.empty()) event.events |= EPOLLOUT;
        event.data.fd = socket;
        if (epoll_ctl(epoll, EPOLL_CTL_MOD, socket, &event)) disconnected();
    }
}

void flipHandler(int, unsigned int, unsigned int, unsigned int, void *data)
{
    static_cast<Display *>(data)->completed();
}

Display::~Display()
{
    /* Drain an outstanding commit before disabling our planes. On timeout close
     * the DRM file first; kernel framebuffer references protect the old pool. */
    if (pending && fd >= 0) {
        pollfd descriptor{fd, POLLIN, 0};
        drmEventContext context{}; context.version = 2; context.page_flip_handler = flipHandler;
        if (poll(&descriptor, 1, 1000) > 0) drmHandleEvent(fd, &context);
    }
    if (pending) { close(fd); fd = -1; }
    if (fd >= 0 && !modeset) {
        drmModeAtomicReq *req = drmModeAtomicAlloc();
        if (req) {
            bool ok = plane(req, ui, 0) && plane(req, video, 0) &&
                add(req, crtc, crtcProps, "ACTIVE", 0) && add(req, crtc, crtcProps, "MODE_ID", 0) &&
                add(req, connector, connectorProps, "CRTC_ID", 0);
            if (!ok || drmModeAtomicCommit(fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, nullptr)) {
                close(fd); fd = -1;
            }
            drmModeAtomicFree(req);
        }
    }
    if (socket >= 0) close(socket);
    for (Buffer &b : videoBuffers) b.release(fd, true);
    for (Buffer &b : uiBuffers) b.release(fd);
    if (fd >= 0) { if (blob) drmModeDestroyPropertyBlob(fd, blob); close(fd); }
    if (epoll >= 0) close(epoll);
}
} // namespace

DisplayThread::~DisplayThread()
{
    stopping_ = true;
    if (wake_ >= 0) { uint64_t value = 1; (void)!write(wake_, &value, sizeof(value)); }
    if (thread_.joinable()) thread_.join();
    if (wake_ >= 0) close(wake_);
}

QSize DisplayThread::start()
{
    wake_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (wake_ < 0) return QSize();
    std::promise<QSize> ready;
    auto future = ready.get_future();
    thread_ = std::thread(&DisplayThread::run, this, std::move(ready));
    return future.get();
}

void DisplayThread::publish(const QImage &image, const QRegion &dirty)
{
    /* Qt's compositor may keep its image actively painted. A deep snapshot here
     * guarantees the display thread never races QPainter's next update. */
    QImage snapshot = image.copy();
    {
        std::lock_guard<std::mutex> guard(mutex_);
        latest_ = std::move(snapshot);
        dirty_ += dirty;
    }
    uint64_t value = 1;
    (void)!write(wake_, &value, sizeof(value));
}

void DisplayThread::run(std::promise<QSize> ready)
{
    Display display;
    if (!display.initialize()) { std::perror("tspi: DRM initialization"); ready.set_value(QSize()); return; }
    epoll_event event{}; event.events = EPOLLIN; event.data.fd = wake_;
    if (epoll_ctl(display.epoll, EPOLL_CTL_ADD, wake_, &event)) { ready.set_value(QSize()); return; }
    ready.set_value(QSize(display.width, display.height));
    drmEventContext context{}; context.version = 2; context.page_flip_handler = flipHandler;
    while (!stopping_) {
        display.connectPreview();
        epoll_event events[4];
        int count = epoll_wait(display.epoll, events, 4, 100);
        if (count < 0 && errno != EINTR) { std::perror("tspi: epoll_wait"); std::abort(); }
        for (int i = 0; i < count; ++i) {
            if (events[i].data.fd == wake_) {
                uint64_t value;
                while (read(wake_, &value, sizeof(value)) > 0) {}
                std::lock_guard<std::mutex> guard(mutex_);
                if (!latest_.isNull()) {
                    display.image = std::move(latest_);
                    for (QRegion &damage : display.damage) damage += dirty_;
                    display.uiDirty = true;
                    dirty_ = QRegion();
                }
            } else if (events[i].data.fd == display.fd) {
                if (drmHandleEvent(display.fd, &context)) { std::perror("tspi: DRM event"); std::abort(); }
            } else if (events[i].data.fd == display.socket) {
                if (events[i].events & EPOLLIN) display.receive();
                if (events[i].events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) display.disconnected();
            }
        }
        display.flushReturns();
        if (!display.submit()) { std::perror("tspi: atomic commit"); std::abort(); }
    }
}
