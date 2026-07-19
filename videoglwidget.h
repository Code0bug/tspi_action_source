#ifndef VIDEOGLWIDGET_H
#define VIDEOGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShader>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QPainter>
#include <QString>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

class VideoGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit VideoGLWidget(QWidget *parent = nullptr);
    ~VideoGLWidget() override;

    // 接收一帧 DMA-BUF fd，宽高，四像素格式（DRM_FORMAT_NV12 等）
    // stride=0 时自动用 w 作为 stride（VI 路径），VDEC 路径需显式传 VirWidth
    // virHeight 用于 plane1 offset 计算：VDEC buffer 有 16 字节对齐 padding，
    //   UV plane 从 stride*virHeight 开始而非 stride*height；VI 路径不传（=0）
    //   时用 m_curH（VI buffer 紧密排布无 padding）
    void setDmaBuf(int fd, int w, int h, void* mbBlk = nullptr, quint32 fourcc = 0,
                   int stride = 0, int virHeight = 0);

    // 释放当前帧的 GL/EGL 资源（切页/隐藏时调用，避免持有过期 DMA-BUF）
    void releaseFrame();

    // OSD 叠加文字（由 paintGL 用 QPainter 直接在 GL widget 上绘制）
    // recording=true 时左上角画红底白字（录像时长），否则无底色
    void setOsdText(const QString& topLeft, const QString& topRight, bool recording = false);

signals:
    // paintGL 完成 EGLImage 绑定后发出，用于通知上游可释放 VI 帧
    void sigReleaseViFrame(void* mbBlk);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void hideEvent(QHideEvent *event) override;

private:
    // EGL 扩展函数指针
    static PFNEGLCREATEIMAGEKHRPROC      s_eglCreateImageKHR;
    static PFNEGLDESTROYIMAGEKHRPROC    s_eglDestroyImageKHR;
    static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC s_glEGLImageTargetTexture2DOES;
    static bool s_extResolved;

    // GL 资源
    GLuint               m_tex      = 0;
    EGLImage             m_image    = nullptr;
    QOpenGLShaderProgram m_program;
    QOpenGLBuffer        m_vbo;

    // 当前帧
int     m_curFd      = -1;
    int     m_curW       = 0;
    int     m_curH       = 0;
    int     m_curStride  = 0;
    int     m_curVirHeight = 0;
    quint32 m_curFourcc = 0;
    void*   m_curMbBlk  = nullptr;

    QString m_osdTopLeft;
    QString m_osdTopRight;
    bool m_osdRecording = false;

    void resolveExt();
    bool createEglImage();
    void destroyEglImage();
};

#endif // VIDEOGLWIDGET_H