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
    initializeOpenGLFunctions(); // 加载OpenGL ES的函数指针
    resolveExt();                // 解析 eglCreateImageKHR 等扩展
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);  // 清屏黑色
    glDisable(GL_DEPTH_TEST);              // 2D 显示不需要深度测试
    glViewport(0, 0, width(), height());   // 设置视口长宽 = 控件大小，告诉OpenGL ES画布有多大

    if (s_glEGLImageTargetTexture2DOES) {
        // 创建纹理对象，生成一个新的纹理ID，存到m_tex中
        glGenTextures(1, &m_tex);  
        // 将后续对该纹理目标的操作绑定到 m_tex
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_tex); 
        // 对纹理目标进行参数配置
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // 解绑当前 GL_TEXTURE_EXTERNAL_OES 目标，避免后续操作误改该纹理
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);    
    }

    // 顶点着色器
    const char *vsrc =
        "attribute vec2 aPos;\n"
        "attribute vec2 aTexCoord;\n"
        "varying vec2 vTexCoord;\n"
        "void main() {\n"
        "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "    vTexCoord = aTexCoord;\n"
        "}\n";
    // 片元着色器
    const char *fsrc =
        "#extension GL_OES_EGL_image_external : require\n"
        "precision mediump float;\n"
        "varying vec2 vTexCoord;\n"
        "uniform samplerExternalOES sTex;\n"
        "void main() {\n"
        "    gl_FragColor = texture2D(sTex, vec2(1.0 - vTexCoord.x, vTexCoord.y));\n"
        "}\n";

    // 编译顶点着色器源码 vsrc，挂到 m_program 上
    if (!m_program.addShaderFromSourceCode(QOpenGLShader::Vertex, vsrc)) {
        qWarning() << "Vertex shader compile failed:" << m_program.log();
    }
    // 编译片元着色器源码 fsrc，挂到 m_program 上
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

    //       屏幕位置坐标     纹理坐标
    //          x  y         u   v
    // 顶点0: (-1,-1) 左下   (0, 0) = 纹理左下角
    // 顶点1: ( 1,-1) 右下   (1, 0) = 纹理右下角
    // 顶点2: (-1, 1) 左上   (0, 1) = 纹理左上角  ──▶ 三角形1: 0→1→2（下半）
    // 顶点3: ( 1, 1) 右上   (1, 1) = 纹理右上角  ──▶ 三角形2: 1→2→3（上半）
    //     2●────●3         2●───●3
    //     │ ╲  ╱│           │    │
    //     │  ╲╱ │    =      │    │     用4个顶点拼出完整矩形
    //     │  ╱╲ │           │    │
    //     │ ╱  ╲│           │    │
    //     0●────●1          0●───●1
    //     三角形条带          两个三角形
    // 1就表示全屏，0.5表示1/2屏
    // 这里就是设置顶点坐标参数，后面会被读取到aPos
    GLfloat verts[] = {
        // x      y     u     v
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
    };
    m_vbo.create();  // 在GPU上申请一块内存
    m_vbo.bind();    // 把这块内存设为当前的 VBO (Vertex Buffer Object，顶点缓冲对象)
    m_vbo.allocate(verts, sizeof(verts)); // 把 verts 数组从 CPU 内存拷贝到 GPU 内存里
    m_vbo.release(); // 解绑
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
        EGL_WIDTH,                       m_curW,               // 图像宽度（像素）
        EGL_HEIGHT,                      (EGLint)m_curH,       // 图像高度（像素）
        EGL_LINUX_DRM_FOURCC_EXT,        (EGLint)fourcc,       // 像素格式四字符码（如 0x3231564e = 'NV12'）
        EGL_DMA_BUF_PLANE0_FD_EXT,       m_curFd,              // Plane0（Y 分量）的 DMA-BUF fd
        EGL_DMA_BUF_PLANE0_OFFSET_EXT,   0,                    // Plane0 在缓冲区中的偏移（数据从 fd 开头放置，偏移为 0）
        EGL_DMA_BUF_PLANE0_PITCH_EXT,    (EGLint)stride,       // Plane0 每行字节数（行跨度）
        EGL_DMA_BUF_PLANE1_FD_EXT,       m_curFd,              // Plane1（UV 分量）的 DMA-BUF fd（NV12 下 Y 和 UV 共用同一 buffer）
        EGL_DMA_BUF_PLANE1_OFFSET_EXT,   (EGLint)plane1Off,    // Plane1（UV）在缓冲区中的起始偏移（= stride * height）
        EGL_DMA_BUF_PLANE1_PITCH_EXT,    (EGLint)stride,       // Plane1 每行字节数（NV12 中 UV 交错排列，stride 同 Plane0）
        EGL_NONE                                               // 属性列表结束标记
    };

    // 获取当前显示句柄
    EGLDisplay dpy = eglGetCurrentDisplay();
    // const char *exts = eglQueryString(dpy, EGL_EXTENSIONS);
    // printf("EGL_EXTS: %s\n", exts ? exts : "(null)");
    // printf("EGL_LINUX_DMA_BUF_EXT=0x%x\n", EGL_LINUX_DMA_BUF_EXT);
    
    // 把dma-buf导入生成EGLImage
    m_image = s_eglCreateImageKHR(dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
                                (EGLClientBuffer)0, attr);
    if (!m_image) {
        qWarning("eglCreateImageKHR failed eglErr=0x%x fd=%d %dx%d fourcc=0x%x",
                eglGetError(), m_curFd, m_curW, m_curH, fourcc);
        return false;
    }   

    if (s_glEGLImageTargetTexture2DOES && m_tex) {
        // 把 m_tex 设为当前操作的纹理对象
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_tex);
        // 把 m_image（EGLImage，包裹着 DMA-BUF 数据）导入到当前绑定的纹理 m_tex 中
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

void VideoGLWidget::hideEvent(QHideEvent *event)
{
    QOpenGLWidget::hideEvent(event);
    if (m_curMbBlk) {
        emit sigReleaseViFrame(m_curMbBlk);
        m_curMbBlk = nullptr;
    }
    releaseFrame();
}

void VideoGLWidget::setDmaBuf(int fd, int w, int h, void* mbBlk, quint32 fourcc,
                               int stride, int virHeight)
{

    // 由相册切回预览时会连续调用三次 capFinishedToUI -> setDmaBuf
    // 但是 QT 会合并多次为一次 paintGL，最后只执行一次 paintGL
    // 这就导致前两次的 mbBlk 没有得到释放，这里是将未 paintGL 的帧进行释放
    if (m_curMbBlk != nullptr && m_curMbBlk != mbBlk) {
        emit sigReleaseViFrame(m_curMbBlk);
    }

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
            m_vbo.bind();  // 重新绑定当前GPU内存为 VBO

            int aPos      = m_program.attributeLocation("aPos");
            int aTexCoord = m_program.attributeLocation("aTexCoord");
            int sTex      = m_program.uniformLocation("sTex");

            glEnableVertexAttribArray(aPos);
            glEnableVertexAttribArray(aTexCoord);
            // 告诉 GPU 如何从 VBO 中解析顶点坐标（aPos）和纹理坐标（aTexCoord）：
            // VBO 每行 4 个 float（16 字节），前 2 个是屏幕位置，后 2 个是纹理位置
            glVertexAttribPointer(aPos,      2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);
            glVertexAttribPointer(aTexCoord, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));

            // 激活纹理单元 0（GPU 有多个纹理槽位，最少 8 个，这里选 0 号）
            glActiveTexture(GL_TEXTURE0);
            // 把 m_tex 绑定为当前操作的纹理对象
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_tex);
            m_program.setUniformValue(sTex, 0);

            // 绘制纹理
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

    // 使用 QPainter 绘图会导致切换预览界面时出现 Segmentation fault (core dumped) 
    // 目前改用 qlabel
#if 0
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
#endif
}