#include "preview_server.h"

#include <rga/im2d.h>
#include <rga/rga.h>
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <linux/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

/* Stable Linux dma-heap UAPI; the RK 4.19 SDK's exported headers predate it. */
struct PreviewHeapAllocation {
    __u64 len;
    __u32 fd, fd_flags;
    __u64 heap_flags;
};
#define PREVIEW_HEAP_ALLOC _IOWR('H', 0, struct PreviewHeapAllocation)

static int openRenderNode()
{
    const char *overrideNode = std::getenv("TSPI_DRM_RENDER");
    if (overrideNode && *overrideNode) return open(overrideNode, O_RDWR | O_CLOEXEC);
    for (int index = 128; index < 160; ++index) {
        char path[64];
        std::snprintf(path, sizeof(path), "/dev/dri/renderD%d", index);
        int fd = open(path, O_RDWR | O_CLOEXEC);
        if (fd < 0) continue;
        drmVersion *version = drmGetVersion(fd);
        bool rockchip = version && version->name && std::strcmp(version->name, "rockchip") == 0;
        drmFreeVersion(version);
        if (rockchip) return fd;
        close(fd);
    }
    return -1;
}

PreviewServer::~PreviewServer()
{
    disconnect();
    if (listener_ >= 0) { close(listener_); unlink(path_.c_str()); }
    if (lock_ >= 0) close(lock_);
}

bool PreviewServer::start()
{
    path_ = preview_socket_path();
    sockaddr_un address{};
    if (path_.size() >= sizeof(address.sun_path)) { errno = ENAMETOOLONG; return false; }
    if (path_ == PREVIEW_SOCKET_PATH) {
        if (mkdir("/run/tspi-camera", 0750) && errno != EEXIST) return false;
        struct stat st{};
        if (lstat("/run/tspi-camera", &st) || !S_ISDIR(st.st_mode) || st.st_uid != geteuid())
            return false;
    }
    lock_ = open((path_ + ".lock").c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0640);
    if (lock_ < 0 || flock(lock_, LOCK_EX | LOCK_NB)) return false;
    struct stat st{};
    if (lstat(path_.c_str(), &st) == 0) {
        if (!S_ISSOCK(st.st_mode) || st.st_uid != geteuid()) { errno = EEXIST; return false; }
        if (unlink(path_.c_str())) return false;
    } else if (errno != ENOENT) return false;
    listener_ = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (listener_ < 0) return false;
    address.sun_family = AF_UNIX;
    std::strcpy(address.sun_path, path_.c_str());
    if (bind(listener_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) ||
        chmod(path_.c_str(), 0660) || listen(listener_, 1)) {
        close(listener_); listener_ = -1;
        unlink(path_.c_str());
        return false;
    }
    std::fprintf(stderr, "preview: listening on %s\n", path_.c_str());
    return true;
}

void PreviewServer::disconnect()
{
    if (client_ >= 0) close(client_);
    client_ = -1;
    /* Never recycle this pool after a lost connection: old GEM handles may still
     * reference its pages until the UI has disabled scanout or has exited. */
    for (int &fd : fds_) { if (fd >= 0) close(fd); fd = -1; }
    busy_.fill(false);
    description_ = preview_packet{};
}

bool PreviewServer::initialize(uint32_t width, uint32_t height)
{
    if (!width || !height || width > 4096 || height > 4096 || (width & 1) || (height & 1))
        return false;
    const char *overrideHeap = std::getenv("TSPI_DMA_HEAP");
    const char *heaps[] = {"/dev/dma_heap/system-uncached-dma32", "/dev/dma_heap/system-dma32",
                           "/dev/dma_heap/system-uncached", "/dev/dma_heap/system",
                           "/dev/dma_heap/cma"};
    int heap = -1;
    if (overrideHeap && *overrideHeap) heap = open(overrideHeap, O_RDWR | O_CLOEXEC);
    else for (const char *path : heaps) {
        heap = open(path, O_RDWR | O_CLOEXEC);
        if (heap >= 0) break;
    }
    int render = -1;
    if (heap < 0 && !(overrideHeap && *overrideHeap)) render = openRenderNode();
    if (heap < 0 && render < 0) { std::perror("preview: dma-heap / Rockchip render node"); return false; }
    description_ = preview_message(PREVIEW_INIT);
    description_.generation = ++generation_;
    description_.width = width;
    description_.height = height;
    description_.pitch = (width + 15u) & ~15u;
    description_.format = DRM_FORMAT_NV12;
    description_.buffer_count = 3;
    description_.buffer_bytes = description_.pitch * height * 3 / 2;
    for (int i = 0; i < 3; ++i) {
        int &fd = fds_[i];
        if (heap >= 0) {
            PreviewHeapAllocation allocation{};
            allocation.len = description_.buffer_bytes;
            allocation.fd_flags = O_RDWR | O_CLOEXEC;
            if (ioctl(heap, PREVIEW_HEAP_ALLOC, &allocation)) {
                std::perror("preview: DMA_HEAP_IOCTL_ALLOC"); close(heap); return false;
            }
            fd = static_cast<int>(allocation.fd);
        } else {
            drm_mode_create_dumb allocation{};
            allocation.width = width; allocation.height = height * 3 / 2; allocation.bpp = 8;
            if (drmIoctl(render, DRM_IOCTL_MODE_CREATE_DUMB, &allocation)) {
                std::perror("preview: render CREATE_DUMB"); close(render); return false;
            }
            int result = drmPrimeHandleToFD(render, allocation.handle, DRM_CLOEXEC | DRM_RDWR, &fd);
            drm_mode_destroy_dumb destroy{}; destroy.handle = allocation.handle;
            drmIoctl(render, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
            if (result || allocation.pitch < width || allocation.size > UINT32_MAX ||
                allocation.size < uint64_t(allocation.pitch) * height * 3 / 2 ||
                (i > 0 && allocation.pitch != description_.pitch)) {
                std::fprintf(stderr, "preview: render dma-buf allocation/export failed\n");
                close(render); return false;
            }
            description_.pitch = allocation.pitch;
            description_.buffer_bytes = allocation.size;
        }
    }
    if (heap >= 0) close(heap);
    if (render >= 0) close(render);
    if (preview_send(client_, &description_, fds_.data()) != 1) return false;
    std::fprintf(stderr, "preview: new pool %llu, NV12 %ux%u stride %u\n",
                 static_cast<unsigned long long>(generation_), width, height, description_.pitch);
    return true;
}

void PreviewServer::pump()
{
    if (listener_ < 0) return;
    /* Bound work even if a peer floods the socket. A live client owns the only
     * pool; additional connections are rejected, never allowed to steal it. */
    int peer = accept4(listener_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (peer >= 0) {
        ucred cred{};
        socklen_t size = sizeof(cred);
        if (client_ >= 0 || getsockopt(peer, SOL_SOCKET, SO_PEERCRED, &cred, &size) ||
            (cred.uid != geteuid() && cred.uid != 0)) close(peer);
        else client_ = peer;
    }
    for (int n = 0; client_ >= 0 && n < 16; ++n) {
        preview_packet packet{};
        int received[3];
        int result = preview_recv(client_, &packet, received);
        if (!result) break;
        for (int fd : received) if (fd >= 0) close(fd);
        if (result < 0) { disconnect(); break; }
        if (packet.type == PREVIEW_HELLO && fds_[0] < 0) {
            if (!initialize(packet.width, packet.height)) disconnect();
        } else if (packet.type == PREVIEW_RELEASE && packet.generation == description_.generation &&
                   packet.slot < 3 && busy_[packet.slot] &&
                   packet.sequence == sequences_[packet.slot]) {
            busy_[packet.slot] = false;
        } else { disconnect(); break; }
    }
}

void PreviewServer::render(const camera_t &camera, const camera_frame_t &frame)
{
    ++sequence_;
    if (client_ < 0 || fds_[0] < 0 || frame.dmabuf_fd < 0 || camera.fmt != V4L2_PIX_FMT_NV12)
        return;
    int slot = 0;
    while (slot < 3 && busy_[slot]) ++slot;
    if (slot == 3) return; // Preview pressure never waits for a UI release.
    rga_buffer_t src = wrapbuffer_fd(frame.dmabuf_fd, int(camera.width), int(camera.height),
                                     RK_FORMAT_YCbCr_420_SP, int(camera.bytesperline), int(camera.height));
    rga_buffer_t out = wrapbuffer_fd(fds_[slot], int(description_.width), int(description_.height),
                                     RK_FORMAT_YCbCr_420_SP, int(description_.pitch), int(description_.height));
    rga_buffer_t pattern{};
    im_rect sr{}, dr{}, pr{};
    sr.width = camera.width; sr.height = camera.height;
    dr.width = description_.width; dr.height = description_.height;
    IM_STATUS status = improcess(src, out, pattern, sr, dr, pr, IM_HAL_TRANSFORM_ROT_90 | IM_SYNC);
    if (status != IM_STATUS_SUCCESS && status != IM_STATUS_NOERROR) {
        std::fprintf(stderr, "preview: RGA failed: %s\n", imStrError(status));
        disconnect(); // Capture continues; the UI can request another pool.
        return;
    }
    preview_packet packet = preview_message(PREVIEW_FRAME);
    packet.generation = description_.generation;
    packet.slot = slot;
    packet.sequence = sequence_;
    packet.timestamp_us = frame.timestamp_us;
    int result = preview_send(client_, &packet, nullptr);
    if (result == 1) { busy_[slot] = true; sequences_[slot] = sequence_; }
    else if (result < 0) disconnect();
    // EAGAIN drops this display frame, with the slot remaining writable.
}
