#ifndef TSPI_DRMSCREEN_H
#define TSPI_DRMSCREEN_H

#include <QImage>
#include <QVector>
#include <QtFbSupport/private/qfbscreen_p.h>
#include <thread>
#include <xf86drmMode.h>

QT_BEGIN_NAMESPACE

enum PlaneType {
    PLANE_PRIMARY,
    PLANE_OVERLAY,
};

struct PlaneProp
{
    uint32_t plane_id = 0;
    uint32_t crtc_id = 0;
    uint32_t fb_id = 0;
    uint32_t crtc_x = 0;
    uint32_t crtc_y = 0;
    uint32_t crtc_w = 0;
    uint32_t crtc_h = 0;
    uint32_t src_x = 0;
    uint32_t src_y = 0;
    uint32_t src_w = 0;
    uint32_t src_h = 0;
    uint32_t zpos = 0;
};

class DrmDevice
{
public:
    struct Framebuffer {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t pitch = 0;
        uint32_t handle = 0;
        uint32_t format = 0;
        uint64_t size = 0;
        void *vaddr = nullptr;
        uint32_t fb_id = 0;
        int prime_fd = -1;
        bool imported = false;
        QImage wrapper;
    };

    DrmDevice();
    ~DrmDevice();

    bool open();
    void close();
    bool createScreens();
    void createFramebuffers();
    bool createSocket();
    void startPreview();
    int fd() const { return m_fd; }

private:
    static const int PRIMARY_BUFFER_COUNT = 2;
    static const int OVERLAY_BUFFER_COUNT = 2;

    bool getCrtcAndConnector();
    bool getPlanes();
    void getProperty(drmModeObjectProperties *props, const char *name,
                     uint32_t *propId, uint64_t *value);
    bool loadPlaneProp(uint32_t planeId, PlaneProp *prop);
    bool createFramebuffer(Framebuffer *buf, uint32_t format, bool map, bool export_fd);
    void destroyFramebuffer(Framebuffer *buf);
    void destroyFramebuffers();
    void previewLoop();
    bool update_plane(PlaneType type, const Framebuffer *buf);

    // DRM 相关资源
    int m_fd;
    drmModeResPtr m_res;
    drmModePlaneResPtr m_planeRes;
    drmModeConnectorPtr m_conn;
    uint32_t m_crtcId;
    uint32_t m_connId;
    PlaneProp m_primary;
    PlaneProp m_overlay;
    // 屏幕宽度/高度
    uint32_t m_width;
    uint32_t m_height;
    // UI 和 video 的 framebuffer
    Framebuffer m_primary_fbs[PRIMARY_BUFFER_COUNT];
    Framebuffer m_overlay_fbs[OVERLAY_BUFFER_COUNT];
    int m_socket;
    std::thread m_previewThread;
};

class TspiDrmScreen : public QFbScreen
{
    Q_OBJECT
public:
    explicit TspiDrmScreen(const QStringList &args);
    ~TspiDrmScreen() override;

    bool initialize() override;
    QRegion doRedraw() override;
    QPixmap grabWindow(WId wid, int x, int y, int width, int height) const override;

private:
    DrmDevice *m_device;
};

QT_END_NAMESPACE

#endif // TSPI_DRMSCREEN_H
