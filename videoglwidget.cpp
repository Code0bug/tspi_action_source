#include "videoglwidget.h"
#include <QSurfaceFormat>

PFNEGLCREATEIMAGEKHRPROC            VideoGLWidget::s_eglCreateImageKHR     = nullptr;
PFNEGLDESTROYIMAGEKHRPROC           VideoGLWidget::s_eglDestroyImageKHR    = nullptr;
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC VideoGLWidget::s_glEGLImageTargetTexture2DOES = nullptr;
bool VideoGLWidget::s_extResolved   = false;

VideoGLWidget::VideoGLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
}

VideoGLWidget::~VideoGLWidget()
{
    if (context()) {
        makeCurrent();
        destroyEglImage();
        doneCurrent();
    }
}

void VideoGLWidget::resolveExt()
{
    if (s_extResolved) return;
    s_eglCreateImageKHR     = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    s_eglDestroyImageKHR    = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    s_glEGLImageTargetTexture2DOES =
        (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    qDebug("VideoGLWidget ext: eglCreateImageKHR=%p eglDestroyImageKHR=%p glEGLImageTargetTexture2DOES=%p",
           s_eglCreateImageKHR, s_eglDestroyImageKHR, s_glEGLImageTargetTexture2DOES);
    s_extResolved = true;
}

void VideoGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    resolveExt();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, width(), height());

    if (s_glEGLImageTargetTexture2DOES) {
        glGenTextures(1, &m_tex);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_tex);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
    }

    // shader：采样 GL_TEXTURE_EXTERNAL_OES（YUV→RGB 由 driver 自动完成）
    // 旋转由 QT_QPA_EGLFS_ROTATION 硬件层完成，shader 不做旋转
    const char *vsrc =
        "attribute vec2 aPos;\n"
        "attribute vec2 aTexCoord;\n"
        "varying vec2 vTexCoord;\n"
        "void main() {\n"
        "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "    vTexCoord = aTexCoord;\n"
        "}\n";
    const char *fsrc =
        "#extension GL_OES_EGL_image_external : require\n"
        "precision mediump float;\n"
        "varying vec2 vTexCoord;\n"
        "uniform samplerExternalOES sTex;\n"
        "void main() {\n"
        "    gl_FragColor = texture2D(sTex, vTexCoord);\n"
        "}\n";

    if (!m_program.addShaderFromSourceCode(QOpenGLShader::Vertex, vsrc)) {
        qWarning() << "Vertex shader compile failed:" << m_program.log();
    }
    if (!m_program.addShaderFromSourceCode(QOpenGLShader::Fragment, fsrc)) {
        qWarning() << "Fragment shader compile failed:" << m_program.log();
    }
    if (!m_program.link()) {
        qWarning() << "Shader link failed:" << m_program.log();
    } else {
        qDebug() << "Shader linked OK. attr aPos=" << m_program.attributeLocation("aPos")
                 << "aTexCoord=" << m_program.attributeLocation("aTexCoord")
                 << "uniform sTex=" << m_program.uniformLocation("sTex");
    }

    // 全屏 quad：两个三角形，4 个顶点，position + texcoord
    // texcoord Y 翻转：GL 纹理坐标 (0,0) 在底部，DMA-BUF 图像 (0,0) 在顶部
    GLfloat verts[] = {
        // x      y     u     v
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
    };
    m_vbo.create();
    m_vbo.bind();
    m_vbo.allocate(verts, sizeof(verts));
    m_vbo.release();
}

void VideoGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

bool VideoGLWidget::createEglImage()
{
    if (!s_eglCreateImageKHR || m_curFd < 0) return false;

    // EGL_EXT_image_dma_buf_import 属性列表
    // EGL_HEIGHT 始终用有效像素高度 m_curH（如 1080），避免对齐 padding 行被采样为绿边。
    // plane1 (UV) offset 取决于 buffer 排布方式：
    //   - VI self-path: buffer 紧密排布，UV 紧贴 Y plane 末尾，offset = stride * height
    //   - VDEC: buffer 有 16 字节对齐 padding，UV 从 stride * virHeight 开始
    //           （virHeight = u32VirHeight，如 1088）
    //   通过 m_curVirHeight 区分：>0 用 virHeight，否则用 m_curH（VI 路径兼容）
    quint32 fourcc    = m_curFourcc ? m_curFourcc : 0x3231564eu; // 'NV12'
    quint32 stride    = m_curStride > 0 ? (quint32)m_curStride : (quint32)m_curW;
    quint32 plane1Off = m_curVirHeight > 0
                        ? stride * (quint32)m_curVirHeight
                        : stride * (quint32)m_curH;

    // 显式声明线性布局 modifier=0 (DRM_FORMAT_MOD_LINEAR)
    EGLint attr[] = {
        EGL_WIDTH,                       m_curW,
        EGL_HEIGHT,                      (EGLint)m_curH,
        EGL_LINUX_DRM_FOURCC_EXT,        (EGLint)fourcc,
        EGL_DMA_BUF_PLANE0_FD_EXT,       m_curFd,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT,   0,
        EGL_DMA_BUF_PLANE0_PITCH_EXT,    (EGLint)stride,
        EGL_DMA_BUF_PLANE1_FD_EXT,       m_curFd,
        EGL_DMA_BUF_PLANE1_OFFSET_EXT,   (EGLint)plane1Off,
        EGL_DMA_BUF_PLANE1_PITCH_EXT,    (EGLint)stride,
        EGL_NONE
    };

    // 用 Qt 当前 GL context 的 EGLDisplay（已 init），但 ctx 必须是 EGL_NO_CONTEXT
    // EGL_EXT_image_dma_buf_import 规范要求：target=EGL_LINUX_DMA_BUF_EXT 时 ctx 必须为 EGL_NO_CONTEXT
    EGLDisplay dpy = eglGetCurrentDisplay();
    const char *exts = eglQueryString(dpy, EGL_EXTENSIONS);
    // printf("EGL_EXTS: %s\n", exts ? exts : "(null)");
    // printf("EGL_LINUX_DMA_BUF_EXT=0x%x\n", EGL_LINUX_DMA_BUF_EXT);
    // 试 buffer=NULL，fd 只在 attr list 里传
    m_image = s_eglCreateImageKHR(dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
                                (EGLClientBuffer)0, attr);
    if (!m_image) {
        qWarning("eglCreateImageKHR failed eglErr=0x%x fd=%d %dx%d fourcc=0x%x",
                eglGetError(), m_curFd, m_curW, m_curH, fourcc);
        return false;
    }   

    if (s_glEGLImageTargetTexture2DOES && m_tex) {
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_tex);
        s_glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, m_image);
    }
    return true;
}

void VideoGLWidget::destroyEglImage()
{
    if (m_image && s_eglDestroyImageKHR) {
        EGLDisplay dpy = eglGetCurrentDisplay();
        s_eglDestroyImageKHR(dpy, m_image);
    }
    m_image = nullptr;
}

void VideoGLWidget::releaseFrame()
{
    if (!context()) return;
    makeCurrent();
    destroyEglImage();
    m_curFd = -1;
    m_curW = m_curH = 0;
    doneCurrent();
}

void VideoGLWidget::setDmaBuf(int fd, int w, int h, void* mbBlk, quint32 fourcc,
                               int stride, int virHeight)
{
    m_curFd       = fd;
    m_curW        = w;
    m_curH        = h;
    m_curStride   = stride;
    m_curVirHeight = virHeight;
    m_curMbBlk    = mbBlk;
    m_curFourcc   = fourcc;
    update();   // 触发 paintGL 重建 EGLImage
}

void VideoGLWidget::setOsdText(const QString &topLeft, const QString &topRight, bool recording)
{
    m_osdTopLeft  = topLeft;
    m_osdTopRight = topRight;
    m_osdRecording = recording;
}

void VideoGLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);

    if (m_curFd >= 0) {
        destroyEglImage();
        if (createEglImage()) {
            m_program.bind();
            m_vbo.bind();

            int aPos      = m_program.attributeLocation("aPos");
            int aTexCoord = m_program.attributeLocation("aTexCoord");
            int sTex      = m_program.uniformLocation("sTex");

            glEnableVertexAttribArray(aPos);
            glEnableVertexAttribArray(aTexCoord);
            glVertexAttribPointer(aPos,      2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);
            glVertexAttribPointer(aTexCoord, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_tex);
            m_program.setUniformValue(sTex, 0);

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            glDisableVertexAttribArray(aPos);
            glDisableVertexAttribArray(aTexCoord);
            m_vbo.release();
            m_program.release();

            if (m_curMbBlk) {
                emit sigReleaseViFrame(m_curMbBlk);
                m_curMbBlk = nullptr;
            }
        }
    }

    // OSD：native GL 绘制完成后，用 QPainter 在 GL widget 上叠加文字（GPU 光栅化）
    glFinish();
    glUseProgram(0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
    glDisable(GL_BLEND);

    QPainter painter(this);
    if (!m_osdTopLeft.isEmpty() || !m_osdTopRight.isEmpty()) {
        painter.setPen(QColor(255, 255, 255));
        QFont font = painter.font();
        font.setPixelSize(25);
        font.setBold(true);
        painter.setFont(font);

        if (!m_osdTopLeft.isEmpty()) {
            QFontMetrics fm(font);
            int textW = fm.horizontalAdvance(m_osdTopLeft);
            int ascent = fm.ascent();
            if (m_osdRecording) {
                painter.fillRect(QRect(20 - 4, 30 - ascent - 2, textW + 8, ascent + fm.descent() + 4), QColor(200, 0, 0));
            }
            painter.drawText(QPoint(20, 30), m_osdTopLeft);
        }
        if (!m_osdTopRight.isEmpty()) {
            QFontMetrics fm(font);
            int textW = fm.horizontalAdvance(m_osdTopRight);
            painter.drawText(QPoint(width() - textW - 20, 30), m_osdTopRight);
        }
    }
    painter.end();
}