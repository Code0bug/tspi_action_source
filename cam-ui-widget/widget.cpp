#include "widget.h"
#include "ui_widget.h"

// 用于fps显示
static char fps_text[16];
static float fps = 0;

QString picture_dirPath = "/userdata/picture";
QString video_dirPath = "/userdata/video";

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    qRegisterMetaType<cv::Mat>("cv::Mat");

    // 假设按钮宽高相等，比如 80x80
    ui->switch_button_0->setFixedSize(80, 80);

    // 设置样式表
    ui->switch_button_0->setStyleSheet(
        "QPushButton {"
        "   background-color: rgba(255, 255, 255, 150);"  // 最后一个参数是透明度
        "   color: black;"
        "   border-radius: 40px;"
        "   border: none;"
        "}"
        "QPushButton:pressed {"
        "   background-color: rgba(255, 255, 255, 200);"
        "}"
        "QPushButton:focus {"
        "   outline: none;"  // 禁用焦点矩形
        "}"
    );

    // 假设按钮宽高相等，比如 80x80
    ui->switch_button_1->setFixedSize(80, 80);

    // 设置样式表
    ui->switch_button_1->setStyleSheet(
        "QPushButton {"
        "   background-color: rgba(255, 255, 255, 200);"
        "   color: black;"
        "   border-radius: 40px;"
        "   border: none;"
        "}"
        "QPushButton:pressed {"
        "   background-color: rgba(255, 255, 255, 230);"
        "}"
        "QPushButton:focus {"
        "   outline: none;"  // 禁用焦点矩形
        "}"
    );

    // 假设按钮宽高相等，比如 80x80
    ui->switch_button_2->setFixedSize(80, 80);

    // 设置样式表
    ui->switch_button_2->setStyleSheet(
        "QPushButton {"
        "   background-color: rgba(255, 255, 255, 200);"
        "   color: black;"
        "   border-radius: 40px;"
        "   border: none;"
        "}"
        "QPushButton:pressed {"
        "   background-color: rgba(255, 255, 255, 230);"
        "}"
        "QPushButton:focus {"
        "   outline: none;"  // 禁用焦点矩形
        "}"
    );

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
    ui->picture_listWidget->setIconSize(QSize(150, 150));     // 图标大小
    ui->picture_listWidget->setGridSize(QSize(180, 200));     // 每个 item 的占位大小
    ui->picture_listWidget->setViewMode(QListView::IconMode); // 图标模式
    ui->picture_listWidget->setResizeMode(QListView::Adjust); // 自动调整

    // 设置视频列表外观
    ui->video_listWidget->setIconSize(QSize(150, 150));
    ui->video_listWidget->setGridSize(QSize(180, 200));
    ui->video_listWidget->setViewMode(QListView::IconMode);
    ui->video_listWidget->setResizeMode(QListView::Adjust);

    // 更新相册列表
    QDir dir(picture_dirPath);
    QStringList filters;
    filters << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp";
    QFileInfoList picture_fileList = dir.entryInfoList(filters, QDir::Files);

    for (auto &file : picture_fileList) {
        QListWidgetItem *item = new QListWidgetItem;
        QPixmap pix(file.absoluteFilePath());
        item->setIcon(QIcon(pix.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        item->setText(file.fileName());
        item->setData(Qt::UserRole, file.absoluteFilePath());
        ui->picture_listWidget->addItem(item);

    }
    // 更新视频列表
    QDir videoDir(video_dirPath);
    QStringList videoFilters;
    videoFilters << "*.bin"; // H.264 原始流
    QFileInfoList videoFileList = videoDir.entryInfoList(videoFilters, QDir::Files);

    for (auto &videoFile : videoFileList) {
        QListWidgetItem *videoItem = new QListWidgetItem;

//        // 可用视频封面图或占位图
//        QPixmap videoPlaceholder("");
//        videoItem->setIcon(QIcon(videoPlaceholder.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        videoItem->setText(videoFile.fileName());
        videoItem->setData(Qt::UserRole, videoFile.absoluteFilePath());

        ui->video_listWidget->addItem(videoItem);
    }


    // 视频采集线程
    cameracapThread = new QThread();
    CameraCapture* cameracap = new CameraCapture();
    cameracap->moveToThread(cameracapThread);

    // 视频编码保存
    savevideoThread = new QThread();
    SaveVideo* savevideo = new SaveVideo();
    savevideo->moveToThread(savevideoThread);

    // 按键控制
    keycontrolThread = new QThread();
    KeyControl* keycontrol = new KeyControl();
    keycontrol->moveToThread(keycontrolThread);

    // 视频解码
    videodecsendThread = new QThread();
    VideoDecSend* videodecsend = new VideoDecSend();
    videodecsend->moveToThread(videodecsendThread);

    videodecgetThread = new QThread();
    VideoDecGet* videodecget = new VideoDecGet();
    videodecget->moveToThread(videodecgetThread);

    // 连接信号
    connect(cameracap, &CameraCapture::capFinishedToUI, this, &Widget::doShow);
    connect(cameracap, &CameraCapture::photoFinishedToUpdate, this, &Widget::updatePicture);
    connect(cameracap, &CameraCapture::videoFinishedToUpdate, this, &Widget::updateVideo);
    connect(cameracap, &CameraCapture::capFinishedToVideo, savevideo, &SaveVideo::do_save_video);

    connect(keycontrol, &KeyControl::keyToTakephoto, cameracap, &CameraCapture::takePhoto, Qt::DirectConnection);
    connect(keycontrol, &KeyControl::keyToStartvideo, cameracap, &CameraCapture::startVideo, Qt::DirectConnection);
    connect(keycontrol, &KeyControl::keyToStopvideo, cameracap, &CameraCapture::stopVideo, Qt::DirectConnection);
    connect(ui->switch_button_0, &QPushButton::clicked, this, &Widget::switchToPage1);
    connect(ui->switch_button_1, &QPushButton::clicked, this, &Widget::switchToPage2);
    connect(ui->switch_button_2, &QPushButton::clicked, this, &Widget::switchToPage0);

    connect(this, &Widget::WidgetToStartCapture, cameracap, &CameraCapture::startCapture);
    connect(this, &Widget::WidgetToStopCapture, cameracap, &CameraCapture::stopCapture);

    connect(keycontrolThread, &QThread::started, keycontrol, &KeyControl::key_query);
    connect(cameracapThread, &QThread::started, cameracap, &CameraCapture::startCapture);

    connect(ui->picture_listWidget, &QListWidget::itemClicked, this, [=](QListWidgetItem *item){
        QString path = item->data(Qt::UserRole).toString();  // 取出存储的完整路径
        QPixmap pix(path);

        ui->picture_label->setPixmap(pix);
    });

    // 点击播放视频
    connect(ui->video_listWidget, &QListWidget::itemClicked, this, [=](QListWidgetItem *item){
        vdec_init(1920, 1080);
        QString path = item->data(Qt::UserRole).toString();  // 取出存储的完整路径
        emit WidgetToStartDecVideo(path);
    });

    connect(this, &Widget::WidgetToStartDecVideo, videodecsend, &VideoDecSend::do_send_frame);
    connect(this, &Widget::WidgetToStartDecVideo, videodecget, &VideoDecGet::do_get_frame);

    connect(videodecget, &VideoDecGet::VideodecToUI, this, &Widget::doShowVideo);

    // 开启线程
    cameracapThread->start();
    savevideoThread->start();
    keycontrolThread->start();
    videodecsendThread->start();
    videodecgetThread->start();

}

Widget::~Widget()
{
    delete ui;
}


void Widget::doShow(cv::Mat img)
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

    sprintf(fps_text, "fps = %.2f", fps);
    cv::putText(img, fps_text,
                    cv::Point(40, 40),
                    cv::FONT_HERSHEY_SIMPLEX,1,
                    cv::Scalar(0,255,0),2);


    QImage image(img.data, img.cols, img.rows, QImage::Format_RGB888);


    ui->video_label->setPixmap(QPixmap::fromImage(image));
}

void Widget::doShowVideo(cv::Mat img)
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

    sprintf(fps_text, "fps = %.2f", fps);
    cv::putText(img, fps_text,
                    cv::Point(40, 40),
                    cv::FONT_HERSHEY_SIMPLEX,1,
                    cv::Scalar(0,255,0),2);


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
    ui->stackedWidget->setCurrentIndex(2);
}

void Widget::switchToPage0() // 切换回摄像头显示
{
    emit WidgetToStartCapture();
    ui->stackedWidget->setCurrentIndex(0);
}

void Widget::updatePicture(QString path)  // 每次拍完照片更新相册
{
    QFileInfo fileInfo(path);
    QString fileName = fileInfo.fileName(); // 提取出文件名称
    QListWidgetItem *item = new QListWidgetItem;
    QPixmap pix(path);

    item->setIcon(QIcon(pix.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    item->setText(fileName);
    item->setData(Qt::UserRole, path);
    ui->picture_listWidget->addItem(item);
}

void Widget::updateVideo(QString path)   // 录像时更新视频列表
{
    QFileInfo fileInfo(path);
    QString fileName = fileInfo.fileName(); // 提取出文件名称
    QListWidgetItem *item = new QListWidgetItem;
    QPixmap pix(path);

    item->setIcon(QIcon(pix.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    item->setText(fileName);
    item->setData(Qt::UserRole, path);
    ui->video_listWidget->addItem(item);

}







