#ifndef TSPI_DRMSCREEN_H
#define TSPI_DRMSCREEN_H

#include <QImage>
#include <QVector>
#include <QtFbSupport/private/qfbscreen_p.h>
#include <xf86drmMode.h>

QT_BEGIN_NAMESPACE

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
    bool importFramebuffers(const int *fds, int count,
                            uint32_t width, uint32_t height, uint32_t pitch);
    int fd() const { return m_fd; }

private:
    static const int BUFFER_COUNT = 3;

    bool getCrtcAndConnector();
    bool getPlanes();
    bool loadPlaneProp(uint32_t planeId, PlaneProp *prop);
    bool createFramebuffer(Framebuffer *buf);
    bool importFramebuffer(Framebuffer *buf, int primeFd);
    void destroyFramebuffer(Framebuffer *buf);
    void destroyFramebuffers();

    int m_fd;
    drmModeResPtr m_res;
    drmModePlaneResPtr m_planeRes;
    drmModeConnectorPtr m_conn;
    uint32_t m_crtcId;
    uint32_t m_connId;
    uint32_t m_width;
    uint32_t m_height;
    PlaneProp m_primary;
    PlaneProp m_overlay;
    Framebuffer m_fbs[BUFFER_COUNT];
    QVector<Framebuffer> m_overlayFbs;
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
    bool receivePreviewBuffers();

    DrmDevice *m_device;
    int m_previewSocket;
};

QT_END_NAMESPACE

#endif // TSPI_DRMSCREEN_H
