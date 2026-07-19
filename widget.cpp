#include "widget.h"
#include "ui_widget.h"
#include "videoglwidget.h"
#include <cstdint>
#include <QLabel>
#include <QFont>

// 用于fps显示
static char fps_text[16];
static float fps = 0;

QString picture_dirPath = "/userdata/picture";
QString video_dirPath = "/userdata/video";
QString firstFrame_dirPath = "/userdata/firstFrame/";

QPixmap makeRoundThumb(const QPixmap &src, int size)
{
    QPixmap dst(size, size);
    dst.fill(Qt::transparent);

    QPainter painter(&dst);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QPainterPath path;
    path.addRoundedRect(0, 0, size, size, 20, 20);   // 圆角20
    painter.setClipPath(path);

    painter.drawPixmap(0, 0, src.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    return dst;
}


static void setupButton(QPushButton *btn, const QString &iconPath)
{
    btn->setIcon(QIcon(QPixmap(iconPath)));
    btn->setFixedSize(80, 80);
    btn->setIconSize(QSize(80, 80));
    btn->setStyleSheet(
        "QPushButton {"
        "  border: none;"
        "  background: transparent;"
        "}"
        "QPushButton:pressed {"
        "  background: transparent;"
        "}"
        "QPushButton:focus {"
        "   outline: none;"
        "}"
    );
}

static void setupOsdLabel(QLabel *lbl, bool recording,
                          int x, int y, int w, int h,
                          Qt::Alignment align)
{
    if (!lbl) return;
    QFont font = lbl->font();
    font.setPixelSize(25);
    font.setBold(true);
    lbl->setFont(font);
    lbl->setAlignment(align);
    lbl->setGeometry(x, y, w, h);
    lbl->setFixedSize(w, h);
    if (recording) {
        lbl->setStyleSheet(
            "QLabel {"
            "  color: white;"
            "  background-color: rgb(200, 0, 0);"
            "  padding: 2px 4px;"
            "}"
        );
    } else {
        lbl->setStyleSheet(
            "QLabel {"
            "  color: white;"
            "  background: transparent;"
            "  padding: 2px 4px;"
            "}"
        );
    }
}

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    qRegisterMetaType<cv::Mat>("cv::Mat");
    qRegisterMetaType<uint64_t>("uint64_t");
    qRegisterMetaType<void*>("void*");

    // 设置按钮样式
    setupButton(ui->photo1back_button,   "/root/back.png");
    setupButton(ui->video1back_button,   "/root/back.png");
    setupButton(ui->switch2video_button, "/root/video1.png");
    setupButton(ui->switch2photo_button, "/root/photo1.png");
    setupButton(ui->photoback_button,    "/root/back.png");
    setupButton(ui->videoback_button,    "/root/back.png");
    setupButton(ui->prev_button,         "/root/chevron-left.png");
    setupButton(ui->next_button,         "/root/chevron-right.png");

    setupOsdLabel(ui->label_time_0, false, 20, 20, 130, 30, Qt::AlignLeft  | Qt::AlignVCenter);
    setupOsdLabel(ui->label_fps_0,  false, 380, 20, 160, 30, Qt::AlignRight | Qt::AlignVCenter);
    setupOsdLabel(ui->label_time_5, true,  20, 20, 90, 30, Qt::AlignLeft  | Qt::AlignVCenter);
    setupOsdLabel(ui->label_fps_5,  false, 380, 20, 160, 30, Qt::AlignRight | Qt::AlignVCenter);

    // 垂直滚动条样式
    ui->picture_listWidget->verticalScrollBar()->setStyleSheet(
        "QScrollBar:vertical {"
        "    width: 40px;"                     // 滚动条整体宽度
        "    background: #f0f0f0;"             // 背景色
        "    margin: 0px 0px 0px 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background: #a0a0a0;"             // 滑块颜色
        "    min-height: 60px;"                // 最小高度，方便触摸
        "    border-radius: 8px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "    background: #808080;"             // 悬停时颜色
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "    height: 0px;"                     // 去掉上下箭头按钮
        "}"
    );

    // 垂直滚动条样式
    ui->video_listWidget->verticalScrollBar()->setStyleSheet(
        "QScrollBar:vertical {"
        "    width: 40px;"                     // 滚动条整体宽度
        "    background: #f0f0f0;"             // 背景色
        "    margin: 0px 0px 0px 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background: #a0a0a0;"             // 滑块颜色
        "    min-height: 60px;"                // 最小高度，方便触摸
        "    border-radius: 8px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "    background: #808080;"             // 悬停时颜色
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "    height: 0px;"                     // 去掉上下箭头按钮
        "}"
    );

    // 设置相册列表外观
    ui->picture_listWidget->setViewMode(QListView::IconMode);
    ui->picture_listWidget->setResizeMode(QListView::Adjust);
    ui->picture_listWidget->setMovement(QListView::Static);
    ui->picture_listWidget->setIconSize(QSize(150,150)); // 缩略图大小
    ui->picture_listWidget->setGridSize(QSize(165,165)); // 缩略图占网格大小
//    ui->picture_listWidget->setStyleSheet(
//        "QListWidget {"
//        "    background-image: url(./photobg.png);"
//        "    background-position: center;"
//        "    background-repeat: no-repeat;"
//        "}"
//        "QListWidget::item {"
//        "    background: rgba(255,255,255,150);"
//        "    border-radius: 16px;"
//        "    margin: 6px;"
//        "}"
//    );

    // 设置视频列表外观
    ui->video_listWidget->setIconSize(QSize(150, 150));
    ui->video_listWidget->setGridSize(QSize(165, 165));
    ui->video_listWidget->setViewMode(QListView::IconMode);
    ui->video_listWidget->setMovement(QListView::Static);
    ui->video_listWidget->setResizeMode(QListView::Adjust);

    // 更新相册列表
    QDir dir(picture_dirPath);
    QStringList filters;
    filters << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp";
    QFileInfoList picture_fileList = dir.entryInfoList(filters, QDir::Files);

    for (auto &file : picture_fileList) {
        QListWidgetItem *item = new QListWidgetItem;

        QPixmap pix(file.absoluteFilePath());
        QPixmap rounded = makeRoundThumb(pix, 160);

        item->setIcon(QIcon(rounded));
//        item->setText(file.fileName());
//        item->setTextAlignment(Qt::AlignCenter); // 让文字更好看
        item->setData(Qt::UserRole, file.absoluteFilePath());

        ui->picture_listWidget->addItem(item);
    }

    // 更新视频列表
    QDir videoDir(video_dirPath);
    QStringList videoFilters;
    videoFilters << "*.mp4";
    QFileInfoList videoFileList = videoDir.entryInfoList(videoFilters, QDir::Files);

    for (auto &videoFile : videoFileList) {
        QListWidgetItem *videoItem = new QListWidgetItem;

        QFileInfo videofileinfo(videoFile.absoluteFilePath());
        QString firstFramePath = firstFrame_dirPath + videofileinfo.completeBaseName() + ".jpg";
        QPixmap pix(firstFramePath);
        QPixmap rounded = makeRoundThumb(pix, 160);

        videoItem->setIcon(QIcon(rounded));
//        videoItem->setText(videoFile.fileName());
        videoItem->setData(Qt::UserRole, videoFile.absoluteFilePath());

        ui->video_listWidget->addItem(videoItem);
    }


    // 视频采集线程
    cameraManagerThread = new QThread();
    CameraManager* cameraManager = new CameraManager();
    cameraManager->moveToThread(cameraManagerThread);

    // 照片保存线程
    savePictureThread = new QThread();
    SavePicture* savePicture = new SavePicture();
    savePicture->moveToThread(savePictureThread);

    // 视频编码保存
    videoEncoderThread = new QThread();
    VideoEncoder* videoEncoder = new VideoEncoder();
    videoEncoder->moveToThread(videoEncoderThread);

    // MP4封装和解封装
    mp4MuxerThread = new QThread();
    Mp4Muxer *mp4Muxer = new Mp4Muxer();
    mp4Muxer->moveToThread(mp4MuxerThread);

    // 音频采集
    audioManagerThread = new QThread();
    AudioManager *audioManager = new AudioManager();
    audioManager->moveToThread(audioManagerThread);

    // 按键控制
    keyControlThread = new QThread();
    KeyControl* keyControl = new KeyControl();
    keyControl->moveToThread(keyControlThread);

    // IMU线程
    imuReaderThread = new QThread();
    IMUReader* imuReader = new IMUReader();
    imuReader->moveToThread(imuReaderThread);

    // 视频解码
    mp4DemuxerThread = new QThread();
    Mp4Demuxer* mp4Demuxer = new Mp4Demuxer();
    mp4Demuxer->moveToThread(mp4DemuxerThread);

    videoDecoderThread = new QThread();
    VideoDecoder* videoDecoder = new VideoDecoder();
    videoDecoder->moveToThread(videoDecoderThread);

    // 连接信号
    connect(cameraManager, &CameraManager::capFinishedToUI, this, &Widget::doShow);                    // 捕获视频到预览显示（fd 零拷贝路径）
    connect(cameraManager, &CameraManager::videoFinishedToUpdate, this, &Widget::updateVideo);         // 录像后更新录像列表
    connect(cameraManager, &CameraManager::videoFinishedToIMURecord, imuReader, &IMUReader::record_imu_data);  // 开始录像时IMU开始记录
    connect(cameraManager, &CameraManager::capFinishedToVideo, videoEncoder, &VideoEncoder::do_save_video);  // 保存录像（异步，videoEncoderThread 事件队列）
    connect(videoEncoder, &VideoEncoder::sigReleaseViFrame, cameraManager, &CameraManager::releaseViFrame, Qt::DirectConnection); // 编码完成释放VI帧
    // VideoGLWidget::paintGL EGLImage 绑定成功后 → frameConsumed → releaseViFrame
    connect(ui->openGLWidget, &VideoGLWidget::sigReleaseViFrame, cameraManager, &CameraManager::releaseViFrame, Qt::DirectConnection);
    // 录像界面 page5 的 openGLWidget_2 由 CameraManager 供帧，释放也回 CameraManager
    connect(ui->openGLWidget_2, &VideoGLWidget::sigReleaseViFrame, cameraManager, &CameraManager::releaseViFrame, Qt::DirectConnection);
    // connect(videoEncoder, &VideoEncoder::sigVideoEncoded, mp4Muxer, &Mp4Muxer::slotWriteVideoEncoded, Qt::QueuedConnection); // 编码流送mp4封装
    connect(cameraManager, &CameraManager::photoFinishedToSave, savePicture, &SavePicture::do_save_picture);  // 保存照片
    connect(savePicture, &SavePicture::photoFinishedToUpdate, this, &Widget::updatePicture);       // 保存照片后更新相册列表

    connect(ui->openGLWidget_1, &VideoGLWidget::sigReleaseViFrame, videoDecoder, &VideoDecoder::releaseVdecFrame, Qt::DirectConnection);
    connect(videoDecoder, &VideoDecoder::VideodecToUI, this, &Widget::doShowVideo);                  // 解码录像并显示（零拷贝路径）

    connect(keyControl, &KeyControl::keyToTakephoto, cameraManager, &CameraManager::takePhoto, Qt::DirectConnection);  // 开始拍照
    connect(keyControl, &KeyControl::keyToStartvideo, cameraManager, &CameraManager::startVideo, Qt::DirectConnection);// 开始录像
    connect(keyControl, &KeyControl::keyToStopvideo, cameraManager, &CameraManager::stopVideo, Qt::DirectConnection);  // 停止录像

    connect(cameraManager, &CameraManager::sigVideoMp4Init, audioManager, &AudioManager::startCapture, Qt::QueuedConnection); // 开始录像时，开启音频采集
    connect(cameraManager, &CameraManager::sigVideoMp4Init, mp4Muxer, &Mp4Muxer::initMp4Mux); // 开始录像时，MP4初始化
    connect(cameraManager, &CameraManager::videoFinishedToMP4, audioManager, &AudioManager::stopCapture, Qt::DirectConnection); // 结束录像时，先停音频采集
    connect(cameraManager, &CameraManager::videoFinishedToMP4, mp4Muxer, &Mp4Muxer::closeMp4Mux); // 结束录像时，再关闭MP4
    connect(videoEncoder, &VideoEncoder::sigVideo2Mp4Mux, mp4Muxer, &Mp4Muxer::writeVideoToMp4);
    connect(audioManager, &AudioManager::capFinishedToAudio, mp4Muxer, &Mp4Muxer::writeAudioToMp4);
    connect(mp4Demuxer, &Mp4Demuxer::audioStreamStarted, audioManager, &AudioManager::startPlayback, Qt::QueuedConnection);
    connect(mp4Demuxer, &Mp4Demuxer::audioPacketReady, audioManager, &AudioManager::playEncodedAudioPacket, Qt::QueuedConnection);
    connect(mp4Demuxer, &Mp4Demuxer::audioStreamFinished, audioManager, &AudioManager::stopPlayback, Qt::QueuedConnection);
    
    connect(keyControl, &KeyControl::keyToStopvideo, imuReader, &IMUReader::stop_imu_record_txt, Qt::DirectConnection);                  // IMU停止记录数据
//    connect(keyControl, &KeyControl::keyToStopvideo, this, &Widget::stop_imu_record);                            // 通知IMU停止记录，关闭文件
    connect(keyControl, &KeyControl::keyToStopvideo, this, &Widget::stopVideo);                                    // 停止录像时标记flag为0
    connect(keyControl, &KeyControl::keyToStartvideo, this, &Widget::switchToPage5);                               // 开始录像时切换到录像页
    connect(keyControl, &KeyControl::keyToStopvideo, this, &Widget::switchToPage0);                                // 停止录像时切回预览页

    connect(ui->switch2photo_button, &QPushButton::clicked, this, &Widget::switchToPage1);  // 切换到相册首页
    connect(ui->switch2video_button, &QPushButton::clicked, this, &Widget::switchToPage2);  // 切换到录像首页
    connect(ui->photoback_button, &QPushButton::clicked, this, &Widget::switchToPage0);     // 从相册首页切换回预览页
    connect(ui->videoback_button, &QPushButton::clicked, this, &Widget::switchToPage0);     // 从录像首页切换回预览页
    connect(ui->video1back_button, &QPushButton::clicked, this, &Widget::switchToPage2);    // 录像播放页回退至录像首页
    connect(ui->video1back_button, &QPushButton::clicked, mp4Demuxer, &Mp4Demuxer::do_stop_send_frame, Qt::DirectConnection);  // 录像播放页回退至录像首页时停止发送解码帧
    connect(ui->video1back_button, &QPushButton::clicked, videoDecoder, &VideoDecoder::do_stop_get_frame, Qt::DirectConnection);    // 录像播放页回退至录像首页时停止获取解码帧
    connect(ui->video1back_button, &QPushButton::clicked, audioManager, &AudioManager::stopPlayback, Qt::QueuedConnection);           // 录像播放页回退至录像首页时停止音频播放
    connect(ui->photo1back_button, &QPushButton::clicked, this, &Widget::switchToPage1);    // 照片查看页回退至相册首页
    connect(ui->next_button, &QPushButton::clicked, this, &Widget::showNextPhoto);          // 照片查看切换下一张
    connect(ui->prev_button, &QPushButton::clicked, this, &Widget::showPrevPhoto);          // 照片查看切换上一张

    connect(ui->picture_listWidget, &QListWidget::itemClicked, this, &Widget::PicListItemClicked);

    // 点击播放视频
    connect(ui->video_listWidget, &QListWidget::itemClicked, this, &Widget::VideoListItemClicked);

    connect(this, &Widget::WidgetToStartCapture, cameraManager, &CameraManager::startCapture);
    connect(this, &Widget::WidgetToStopCapture, cameraManager, &CameraManager::stopCapture, Qt::DirectConnection);
    connect(this, &Widget::WidgetToStartDecVideo, mp4Demuxer, &Mp4Demuxer::do_send_frame);
    connect(this, &Widget::WidgetToStartDecVideo, videoDecoder, &VideoDecoder::do_get_frame);

    connect(keyControlThread, &QThread::started, keyControl, &KeyControl::key_query);
    connect(cameraManagerThread, &QThread::started, cameraManager, &CameraManager::startCapture);

//    connect(&IMUTimer, &QTimer::timeout, imuReader, &IMUReader::record_imu_data);
//    connect(imuReader, &IMUReader::IMUinitToWidget, this, &Widget::start_imu_record); // 开始录制后，IMU要先记录时间戳，初始化计数变量，然后才开启定时器进行记录

    video_flag = 0; // video_flag初始化为0，不初始化会有个野值
    // 开启线程
    cameraManagerThread->start();
    savePictureThread->start();
    videoEncoderThread->start();
    keyControlThread->start();
    mp4DemuxerThread->start();
    videoDecoderThread->start();
    imuReaderThread->start();
    mp4MuxerThread->start();
    audioManagerThread->start();

}

Widget::~Widget()
{
    // 通知所有线程停止
    cameraManagerThread->requestInterruption();
    cameraManagerThread->quit();
    keyControlThread->quit();
    savePictureThread->quit();
    videoEncoderThread->quit();
    mp4DemuxerThread->quit();
    videoDecoderThread->quit();
    imuReaderThread->quit();
    mp4MuxerThread->quit();
    audioManagerThread->quit();

    // 等待线程结束，超时则强制终止
    QThread *threads[] = {
        cameraManagerThread, keyControlThread, savePictureThread,
        videoEncoderThread, mp4DemuxerThread, videoDecoderThread,
        imuReaderThread, mp4MuxerThread, audioManagerThread
    };
    for (QThread *t : threads) {
        if (!t->wait(2000)) {
            t->terminate();
            t->wait(1000);
        }
    }

    delete ui;
}

void Widget::doShow(int fd, int w, int h, void* mbBlk, uint64_t pts)  // 预览（VideoGLWidget 零拷贝路径）
{
    // FPS 统计
    static int64_t time_last = 0;
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    int64_t timestamp = duration.count();
    int64_t fps_time = timestamp - time_last;
    if (fps_time > 0) fps = 1000 / fps_time;
    time_last = timestamp;
    sprintf(fps_text, "FPS:%.0f", fps);
    QString fpsStr = QString::fromUtf8(fps_text);

    // 左上 OSD：录像时画时长，非录像画系统时间
    QString timeStr;
    if (video_flag) {
        QDateTime nowT = QDateTime::currentDateTime();
        qint64 elapsedMS = recordStartTime.msecsTo(nowT);
        int seconds = (elapsedMS / 1000) % 60;
        int minutes = (elapsedMS / (1000 * 60)) % 60;
        timeStr = QString("%1:%2")
                     .arg(minutes, 2, 10, QChar('0'))
                     .arg(seconds, 2, 10, QChar('0'));
    } else {
        timeStr = QDateTime::currentDateTime().toString("HH'h'mm'm'");
    }

    int curPage = ui->stackedWidget->currentIndex();
    if (curPage == 5) {
        ui->openGLWidget_2->setDmaBuf(fd, w, h, mbBlk);
        ui->label_time_5->setText(timeStr);
        ui->label_fps_5->setText(fpsStr);
    } else {
        ui->openGLWidget->setDmaBuf(fd, w, h, mbBlk);
        ui->label_time_0->setText(timeStr);
        ui->label_fps_0->setText(fpsStr);
    }

}

void Widget::doShowVideo(int fd, int w, int h, void* mbBlk, int stride, int virHeight)  // 播放录像（VDEC 零拷贝路径）
{
    if (ui->openGLWidget_1) {
        ui->openGLWidget_1->setDmaBuf(fd, w, h, mbBlk, 0, stride, virHeight);
    }
}


void Widget::switchToPage1() // 切换到相册列表
{
    emit WidgetToStopCapture();
    ui->stackedWidget->setCurrentIndex(1);
}

void Widget::switchToPage2() // 切换到视频列表
{
    emit WidgetToStopCapture();
    ui->stackedWidget->setCurrentIndex(2);
}

void Widget::switchToPage0() // 切换回摄像头显示
{
    int prev = ui->stackedWidget->currentIndex();
    ui->stackedWidget->setCurrentIndex(0);
    // 仅从相册/视频列表返回时重启采集；从录像停止（page5）返回时采集仍在运行，避免重复 start
    if (prev == 1 || prev == 2) {
        emit WidgetToStartCapture();
    }
}

void Widget::switchToPage5() // 切换至录像界面
{
    ui->stackedWidget->setCurrentIndex(5);
}

void Widget::updatePicture(QString path)  // 每次拍完照片更新相册
{
    QFileInfo fileInfo(path);
    QString fileName = fileInfo.fileName(); // 提取出文件名称
    QListWidgetItem *item = new QListWidgetItem;
    QPixmap pix(path);
    QPixmap rounded = makeRoundThumb(pix, 160);
    item->setIcon(QIcon(rounded));
//    item->setText(fileName);
    item->setData(Qt::UserRole, path);
    ui->picture_listWidget->addItem(item);
}

void Widget::updateVideo(QString path, cv::Mat first_frame)   // 录像时更新视频列表
{
    QFileInfo fileInfo(path);
    QString fileName = fileInfo.fileName(); // 提取出文件名称
    QListWidgetItem *item = new QListWidgetItem;

    QImage image(first_frame.data, first_frame.cols, first_frame.rows, QImage::Format_BGR888);

    QPixmap rounded = makeRoundThumb(QPixmap::fromImage(image), 160);
    item->setIcon(QIcon(rounded));
//    item->setText(fileName);
    item->setData(Qt::UserRole, path);
    ui->video_listWidget->addItem(item);

    video_flag = 1; // 开始录像
    recordStartTime = QDateTime::currentDateTime(); // 记录下录制的时间
}

void Widget::stopVideo()
{
    video_flag = 0;
}


void Widget::showNextPhoto()
{
    picture_current_index++;
    if (picture_current_index >= ui->picture_listWidget->count()){
        picture_current_index = 0;
    }

    QListWidgetItem *item = ui->picture_listWidget->item(picture_current_index);
    QString path = item->data(Qt::UserRole).toString();
//    qDebug() << "index " << picture_current_index << " path " << path;

    QPixmap pix(path);
    ui->picture_label->setPixmap(pix);
}

void Widget::showPrevPhoto()
{
    picture_current_index--;

    if (picture_current_index < 0) {
        picture_current_index = ui->picture_listWidget->count() - 1;
    }

    QListWidgetItem *item = ui->picture_listWidget->item(picture_current_index);
    QString path = item->data(Qt::UserRole).toString();
//    qDebug() << "index " << picture_current_index << " path " << path;

    QPixmap pix(path);
    ui->picture_label->setPixmap(pix);
}

// 相册列表item被点击时的动作
void Widget::PicListItemClicked(QListWidgetItem *item)
{
    picture_current_index = ui->picture_listWidget->row(item);  // 读取当前图片索引
//    qDebug() << "index : " << picture_current_index;
    QString path = item->data(Qt::UserRole).toString();  // 取出存储的完整路径
    QPixmap pix(path);

    ui->stackedWidget->setCurrentIndex(3);  // 切换到照片显示页

    ui->picture_label->setPixmap(pix);
}

// 录像列表item被点击时的动作
void Widget::VideoListItemClicked(QListWidgetItem *item)
{
    // VDEC 通道的创建/销毁已移至 VideoDecoder worker 线程内（reinitVdec/destroyVdec），
    // 避免与 do_get_frame 的 DestroyChn 跨线程竞争
    ui->stackedWidget->setCurrentIndex(4);  // 切换到录像显示页
    QString path = item->data(Qt::UserRole).toString();  // 取出存储的完整路径
    emit WidgetToStartDecVideo(path);
}
