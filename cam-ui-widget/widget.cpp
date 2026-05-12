#include "widget.h"
#include "ui_widget.h"
#include <cstdint>

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

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    qRegisterMetaType<cv::Mat>("cv::Mat");
    qRegisterMetaType<uint64_t>("uint64_t");

    // 设置按钮样式
    setupButton(ui->photo1back_button,   "/root/back.png");
    setupButton(ui->video1back_button,   "/root/back.png");
    setupButton(ui->switch2video_button, "/root/video1.png");
    setupButton(ui->switch2photo_button, "/root/photo1.png");
    setupButton(ui->photoback_button,    "/root/back.png");
    setupButton(ui->videoback_button,    "/root/back.png");
    setupButton(ui->prev_button,         "/root/chevron-left.png");
    setupButton(ui->next_button,         "/root/chevron-right.png");

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
    cameracapThread = new QThread();
    CameraCapture* cameracap = new CameraCapture();
    cameracap->moveToThread(cameracapThread);

    // 照片保存线程
    savepictureThread = new QThread();
    SavePicture* savepicture = new SavePicture();
    savepicture->moveToThread(savepictureThread);

    // 视频编码保存
    savevideoThread = new QThread();
    SaveVideo* savevideo = new SaveVideo();
    savevideo->moveToThread(savevideoThread);

    // MP4封装和解封装
    mp4muxThread = new QThread();
    Mp4MuxDemux *mp4mux = new Mp4MuxDemux();
    mp4mux->moveToThread(mp4muxThread);

    // 音频采集
    audiocapThread = new QThread();
    AudioManager *audiocap = new AudioManager();
    audiocap->moveToThread(audiocapThread);

    // 按键控制
    keycontrolThread = new QThread();
    KeyControl* keycontrol = new KeyControl();
    keycontrol->moveToThread(keycontrolThread);

    // IMU线程
    imuThread = new QThread();
    IMUReader* imureader = new IMUReader();
    imureader->moveToThread(imuThread);

    // 视频解码
    videodecsendThread = new QThread();
    VideoDecSend* videodecsend = new VideoDecSend();
    videodecsend->moveToThread(videodecsendThread);

    videodecgetThread = new QThread();
    VideoDecGet* videodecget = new VideoDecGet();
    videodecget->moveToThread(videodecgetThread);

    // 连接信号
    connect(cameracap, &CameraCapture::capFinishedToUI, this, &Widget::doShow);                    // 捕获视频到预览显示
    connect(cameracap, &CameraCapture::videoFinishedToUpdate, this, &Widget::updateVideo);         // 录像后更新录像列表
    connect(cameracap, &CameraCapture::videoFinishedToIMURecord, imureader, &IMUReader::record_imu_data);  // 开始录像时IMU开始记录
    connect(cameracap, &CameraCapture::capFinishedToVideo, savevideo, &SaveVideo::do_save_video);  // 保存录像
    // connect(savevideo, &SaveVideo::sigVideoEncoded, mp4mux, &Mp4MuxDemux::slotWriteVideoEncoded, Qt::QueuedConnection); // 编码流送mp4封装
    connect(cameracap, &CameraCapture::photoFinishedToSave, savepicture, &SavePicture::do_save_picture);  // 保存照片
    connect(savepicture, &SavePicture::photoFinishedToUpdate, this, &Widget::updatePicture);       // 保存照片后更新相册列表


    connect(videodecget, &VideoDecGet::VideodecToUI, this, &Widget::doShowVideo);                  // 解码录像并显示

    connect(keycontrol, &KeyControl::keyToTakephoto, cameracap, &CameraCapture::takePhoto, Qt::DirectConnection);  // 开始拍照
    connect(keycontrol, &KeyControl::keyToStartvideo, cameracap, &CameraCapture::startVideo, Qt::DirectConnection);// 开始录像
    connect(keycontrol, &KeyControl::keyToStopvideo, cameracap, &CameraCapture::stopVideo, Qt::DirectConnection);  // 停止录像

    connect(cameracap, &CameraCapture::sigVideoMp4Init, audiocap, &AudioManager::startCapture, Qt::QueuedConnection); // 开始录像时，开启音频采集
    connect(cameracap, &CameraCapture::sigVideoMp4Init, mp4mux, &Mp4MuxDemux::initMp4Mux); // 开始录像时，MP4初始化
    connect(cameracap, &CameraCapture::videoFinishedToMP4, audiocap, &AudioManager::stopCapture, Qt::DirectConnection); // 结束录像时，先停音频采集
    connect(cameracap, &CameraCapture::videoFinishedToMP4, mp4mux, &Mp4MuxDemux::closeMp4Mux); // 结束录像时，再关闭MP4
    connect(savevideo, &SaveVideo::sigVideo2Mp4Mux, mp4mux, &Mp4MuxDemux::writeVideoToMp4);
    connect(audiocap, &AudioManager::capFinishedToAudio, mp4mux, &Mp4MuxDemux::writeAudioToMp4);
    connect(videodecsend, &VideoDecSend::audioStreamStarted, audiocap, &AudioManager::startPlayback, Qt::QueuedConnection);
    connect(videodecsend, &VideoDecSend::audioPacketReady, audiocap, &AudioManager::playEncodedAudioPacket, Qt::QueuedConnection);
    connect(videodecsend, &VideoDecSend::audioStreamFinished, audiocap, &AudioManager::stopPlayback, Qt::QueuedConnection);
    
    connect(keycontrol, &KeyControl::keyToStopvideo, imureader, &IMUReader::stop_imu_record_txt, Qt::DirectConnection);                  // IMU停止记录数据
//    connect(keycontrol, &KeyControl::keyToStopvideo, this, &Widget::stop_imu_record);                            // 通知IMU停止记录，关闭文件
    connect(keycontrol, &KeyControl::keyToStopvideo, this, &Widget::stopVideo);                                    // 停止录像时标记flag为0
    connect(keycontrol, &KeyControl::keyToStartvideo, this, &Widget::switchToPage5);                               // 开始录像时切换到录像页
    connect(keycontrol, &KeyControl::keyToStopvideo, this, &Widget::switchToPage0);                                // 停止录像时切回预览页

    connect(ui->switch2photo_button, &QPushButton::clicked, this, &Widget::switchToPage1);  // 切换到相册首页
    connect(ui->switch2video_button, &QPushButton::clicked, this, &Widget::switchToPage2);  // 切换到录像首页
    connect(ui->photoback_button, &QPushButton::clicked, this, &Widget::switchToPage0);     // 从相册首页切换回预览页
    connect(ui->videoback_button, &QPushButton::clicked, this, &Widget::switchToPage0);     // 从录像首页切换回预览页
    connect(ui->video1back_button, &QPushButton::clicked, this, &Widget::switchToPage2);    // 录像播放页回退至录像首页
    connect(ui->video1back_button, &QPushButton::clicked, videodecsend, &VideoDecSend::do_stop_send_frame, Qt::DirectConnection);  // 录像播放页回退至录像首页时停止发送解码帧
    connect(ui->video1back_button, &QPushButton::clicked, videodecget, &VideoDecGet::do_stop_get_frame, Qt::DirectConnection);    // 录像播放页回退至录像首页时停止获取解码帧
    connect(ui->video1back_button, &QPushButton::clicked, audiocap, &AudioManager::stopPlayback, Qt::QueuedConnection);           // 录像播放页回退至录像首页时停止音频播放
    connect(ui->photo1back_button, &QPushButton::clicked, this, &Widget::switchToPage1);    // 照片查看页回退至相册首页
    connect(ui->next_button, &QPushButton::clicked, this, &Widget::showNextPhoto);          // 照片查看切换下一张
    connect(ui->prev_button, &QPushButton::clicked, this, &Widget::showPrevPhoto);          // 照片查看切换上一张

    connect(ui->picture_listWidget, &QListWidget::itemClicked, this, &Widget::PicListItemClicked);

    // 点击播放视频
    connect(ui->video_listWidget, &QListWidget::itemClicked, this, &Widget::VideoListItemClicked);

    connect(this, &Widget::WidgetToStartCapture, cameracap, &CameraCapture::startCapture);
    connect(this, &Widget::WidgetToStopCapture, cameracap, &CameraCapture::stopCapture);
    connect(this, &Widget::WidgetToStartDecVideo, videodecsend, &VideoDecSend::do_send_frame);
    connect(this, &Widget::WidgetToStartDecVideo, videodecget, &VideoDecGet::do_get_frame);

    connect(keycontrolThread, &QThread::started, keycontrol, &KeyControl::key_query);
    connect(cameracapThread, &QThread::started, cameracap, &CameraCapture::startCapture);

//    connect(&IMUTimer, &QTimer::timeout, imureader, &IMUReader::record_imu_data);
//    connect(imureader, &IMUReader::IMUinitToWidget, this, &Widget::start_imu_record); // 开始录制后，IMU要先记录时间戳，初始化计数变量，然后才开启定时器进行记录

    video_flag = 0; // video_flag初始化为0，不初始化会有个野值
    // 开启线程
    cameracapThread->start();
    savepictureThread->start();
    savevideoThread->start();
    keycontrolThread->start();
    videodecsendThread->start();
    videodecgetThread->start();
    imuThread->start();
    mp4muxThread->start();
    audiocapThread->start();

}

Widget::~Widget()
{
    delete ui;
}

//void Widget::start_imu_record()
//{
//    IMUTimer.start(20); // 20ms->50hz
//}

//void Widget::stop_imu_record()
//{
//    IMUTimer.stop();
//}


void Widget::doShow(cv::Mat img)  // 预览和录像
{

    static int64_t time_last = 0;
    // 获取当前时间点
    auto now = std::chrono::system_clock::now();
    // 转换为自1970年1月1日以来的毫秒数
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    // 获取毫秒级时间戳
    int64_t timestamp = duration.count();
    int64_t fps_time = timestamp - time_last;
    fps = 1000 / fps_time;

    time_last = timestamp;

    sprintf(fps_text, "FPS:%.0f", fps);
    cv::putText(img, fps_text,
                    cv::Point(420, 40),
                    cv::FONT_HERSHEY_SIMPLEX,1,
                    cv::Scalar(220,220,220),2);

    if (video_flag) { // 录像时，显示录像时长

        QDateTime now = QDateTime::currentDateTime();
        qint64 elapsedMS = recordStartTime.msecsTo(now);  // 毫秒
        int seconds = (elapsedMS / 1000) % 60;
        int minutes = (elapsedMS / (1000*60)) % 60;

        // 构造分钟和秒的字符串
        QString elapsedTime = QString("%1:%2")
                                .arg(minutes, 2, 10, QChar('0'))
                                .arg(seconds, 2, 10, QChar('0'));

        // 转 std::string
        std::string text = elapsedTime.toStdString();

        // 获取文字尺寸
        int fontFace = cv::FONT_HERSHEY_SIMPLEX;
        double fontScale = 1;
        int thickness = 2;
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(text, fontFace, fontScale, thickness, &baseline);

        // 绘制红色背景矩形
        cv::Point textOrg(20, 40);
        cv::rectangle(img,
                      textOrg + cv::Point(-3, baseline),
                      textOrg + cv::Point(textSize.width, -textSize.height-5),
                      cv::Scalar(0, 0, 255),
                      cv::FILLED);

        // 绘制白色文字
        cv::putText(img, text, textOrg, fontFace, fontScale, cv::Scalar(220, 220, 220), thickness);

        QImage image(img.data, img.cols, img.rows, QImage::Format_BGR888);
        ui->video_label_3->setPixmap(QPixmap::fromImage(image));

    } else {  // 非录像下，显示系统时间
        QString timeStr = QDateTime::currentDateTime().toString("HH'h'mm'm'");
        cv::putText(img, timeStr.toStdString(),
                    cv::Point(20, 40),
                    cv::FONT_HERSHEY_SIMPLEX, 1,
                    cv::Scalar(220,220,220), 2);

        QImage image(img.data, img.cols, img.rows, QImage::Format_BGR888);
        ui->video_label->setPixmap(QPixmap::fromImage(image));
    }



}

void Widget::doShowVideo(cv::Mat img)  // 播放录像
{

    QImage image(img.data, img.cols, img.rows, QImage::Format_RGB888);

    ui->video_label_2->setPixmap(QPixmap::fromImage(image));
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
    ui->stackedWidget->setCurrentIndex(0);
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
    vdec_init(1920, 1080);

    ui->stackedWidget->setCurrentIndex(4);  // 切换到录像显示页
    QString path = item->data(Qt::UserRole).toString();  // 取出存储的完整路径
    emit WidgetToStartDecVideo(path);
}
