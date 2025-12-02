#include "widget.h"
#include "ui_widget.h"

// 用于fps显示
static char fps_text[16];
static float fps = 0;

QString picture_dirPath = "/userdata/picture";
QString video_dirPath = "/userdata/video";

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


Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    qRegisterMetaType<cv::Mat>("cv::Mat");

//    ui->video_label->setStyleSheet("text-align:center;font-size:1px;");
//    ui->video_label->setContentsMargins(0,0,0,0);
//    ui->stackedWidget->setContentsMargins(0,0,0,0);
//    ui->page0->setContentsMargins(0,0,0,0);

    QPixmap photo1back_button_pix("./back.png");  // 加载图标
    ui->photo1back_button->setIcon(QIcon(photo1back_button_pix));
    ui->photo1back_button->setFixedSize(80, 80);     // 设置点击区域大小
    ui->photo1back_button->setIconSize(QSize(80, 80)); // 设置图标大小
    ui->photo1back_button->setStyleSheet( // 设置原按钮透明
        "QPushButton {"
        "  border: none;"
        "  background: transparent;"
        "}"
        "QPushButton:pressed {"
        "  background: transparent;"
        "}"
        "QPushButton:focus {"
        "   outline: none;"  // 禁用焦点矩形
        "}"
    );

    QPixmap switch2video_button_pix("./video1.png");  // 加载图标
    ui->switch2video_button->setIcon(QIcon(switch2video_button_pix));
    ui->switch2video_button->setFixedSize(80, 80);     // 设置点击区域大小
    ui->switch2video_button->setIconSize(QSize(80, 80)); // 设置图标大小
    ui->switch2video_button->setStyleSheet( // 设置原按钮透明
        "QPushButton {"
        "  border: none;"
        "  background: transparent;"
        "}"
        "QPushButton:pressed {"
        "  background: transparent;"
        "}"
        "QPushButton:focus {"
        "   outline: none;"  // 禁用焦点矩形
        "}"
    );

    QPixmap switch2photo_button_pix("./photo1.png");  // 加载图标
    ui->switch2photo_button->setIcon(QIcon(switch2photo_button_pix));
    ui->switch2photo_button->setFixedSize(80, 80);     // 设置点击区域大小
    ui->switch2photo_button->setIconSize(QSize(80, 80)); // 设置图标大小
    ui->switch2photo_button->setStyleSheet( // 设置原按钮透明
        "QPushButton {"
        "  border: none;"
        "  background: transparent;"
        "}"
        "QPushButton:pressed {"
        "  background: transparent;"
        "}"
        "QPushButton:focus {"
        "   outline: none;"  // 禁用焦点矩形
        "}"
    );

    QPixmap photoback_button_pix("./back.png");  // 加载图标
    ui->photoback_button->setIcon(QIcon(photoback_button_pix));
    ui->photoback_button->setFixedSize(80, 80);     // 设置点击区域大小
    ui->photoback_button->setIconSize(QSize(80, 80)); // 设置图标大小
    ui->photoback_button->setStyleSheet( // 设置原按钮透明
        "QPushButton {"
        "  border: none;"
        "  background: transparent;"
        "}"
        "QPushButton:pressed {"
        "  background: transparent;"
        "}"
        "QPushButton:focus {"
        "   outline: none;"  // 禁用焦点矩形
        "}"
    );


    QPixmap videoback_button_pix("./back.png");  // 加载图标
    ui->videoback_button->setIcon(QIcon(videoback_button_pix));
    ui->videoback_button->setFixedSize(80, 80);     // 设置点击区域大小
    ui->videoback_button->setIconSize(QSize(80, 80)); // 设置图标大小
    ui->videoback_button->setStyleSheet( // 设置原按钮透明
        "QPushButton {"
        "  border: none;"
        "  background: transparent;"
        "}"
        "QPushButton:pressed {"
        "  background: transparent;"
        "}"
        "QPushButton:focus {"
        "   outline: none;"  // 禁用焦点矩形
        "}"
    );

    QPixmap prev_button_pix("./chevron-left.png");  // 加载图标
    ui->prev_button->setIcon(QIcon(prev_button_pix));
    ui->prev_button->setFixedSize(80, 80);     // 设置点击区域大小
    ui->prev_button->setIconSize(QSize(80, 80)); // 设置图标大小
    ui->prev_button->setStyleSheet( // 设置原按钮透明
        "QPushButton {"
        "  border: none;"
        "  background: transparent;"
        "}"
        "QPushButton:pressed {"
        "  background: transparent;"
        "}"
        "QPushButton:focus {"
        "   outline: none;"  // 禁用焦点矩形
        "}"
    );

    QPixmap next_button_pix("./chevron-right.png");  // 加载图标
    ui->next_button->setIcon(QIcon(next_button_pix));
    ui->next_button->setFixedSize(80, 80);     // 设置点击区域大小
    ui->next_button->setIconSize(QSize(80, 80)); // 设置图标大小
    ui->next_button->setStyleSheet( // 设置原按钮透明
        "QPushButton {"
        "  border: none;"
        "  background: transparent;"
        "}"
        "QPushButton:pressed {"
        "  background: transparent;"
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
    ui->video_listWidget->setGridSize(QSize(180, 200));
    ui->video_listWidget->setViewMode(QListView::IconMode);
    ui->video_listWidget->setResizeMode(QListView::Adjust);

    // 更新相册列表
    QDir dir(picture_dirPath);
    QStringList filters;
    filters << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp";
    QFileInfoList picture_fileList = dir.entryInfoList(filters, QDir::Files);

//    for (auto &file : picture_fileList) {
//        QListWidgetItem *item = new QListWidgetItem;
//        QPixmap pix(file.absoluteFilePath());
//        item->setIcon(QIcon(pix.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
//        item->setText(file.fileName());
//        item->setData(Qt::UserRole, file.absoluteFilePath());
//        ui->picture_listWidget->addItem(item);

//    }
    int thumbSize = 160;

    for (auto &file : picture_fileList) {
        QListWidgetItem *item = new QListWidgetItem;

        QPixmap pix(file.absoluteFilePath());
        QPixmap rounded = makeRoundThumb(pix, thumbSize);

        item->setIcon(QIcon(rounded));
//        item->setText(file.fileName());
//        item->setTextAlignment(Qt::AlignCenter); // 让文字更好看
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
    connect(cameracap, &CameraCapture::capFinishedToUI, this, &Widget::doShow);                    // 捕获视频到预览显示
    connect(cameracap, &CameraCapture::photoFinishedToUpdate, this, &Widget::updatePicture);       // 拍照后更新相册列表
    connect(cameracap, &CameraCapture::videoFinishedToUpdate, this, &Widget::updateVideo);         // 录像后更新录像列表
    connect(cameracap, &CameraCapture::capFinishedToVideo, savevideo, &SaveVideo::do_save_video);  // 保存录像

    connect(videodecget, &VideoDecGet::VideodecToUI, this, &Widget::doShowVideo);                  // 解码录像并显示

    connect(keycontrol, &KeyControl::keyToTakephoto, cameracap, &CameraCapture::takePhoto, Qt::DirectConnection);  // 开始拍照
    connect(keycontrol, &KeyControl::keyToStartvideo, cameracap, &CameraCapture::startVideo, Qt::DirectConnection);// 开始录像
    connect(keycontrol, &KeyControl::keyToStopvideo, cameracap, &CameraCapture::stopVideo, Qt::DirectConnection);  // 停止录像
    connect(keycontrol, &KeyControl::keyToStopvideo, this, &Widget::stopVideo);                                    // 停止录像时标记flag为0
    connect(keycontrol, &KeyControl::keyToStartvideo, this, &Widget::switchToPage5);                               // 开始录像时切换到录像页
    connect(keycontrol, &KeyControl::keyToStopvideo, this, &Widget::switchToPage0);                                // 停止录像时切回预览页

    connect(ui->switch2photo_button, &QPushButton::clicked, this, &Widget::switchToPage1);  // 切换到相册首页
    connect(ui->switch2video_button, &QPushButton::clicked, this, &Widget::switchToPage2);  // 切换到录像首页
    connect(ui->photoback_button, &QPushButton::clicked, this, &Widget::switchToPage0);     // 从相册首页切换回预览页
    connect(ui->videoback_button, &QPushButton::clicked, this, &Widget::switchToPage0);     // 从录像首页切换回预览页
    connect(ui->photo1back_button, &QPushButton::clicked, this, &Widget::switchToPage1);    // 照片查看页回退至相册首页
    connect(ui->next_button, &QPushButton::clicked, this, &Widget::showNextPhoto);          // 照片查看切换下一张
    connect(ui->prev_button, &QPushButton::clicked, this, &Widget::showPrevPhoto);          // 照片查看切换上一张

    connect(ui->picture_listWidget, &QListWidget::itemClicked, this, [=](QListWidgetItem *item){

        picture_current_index = ui->picture_listWidget->row(item);  // 读取当前图片索引
        qDebug() << "index : " << picture_current_index;
        QString path = item->data(Qt::UserRole).toString();  // 取出存储的完整路径
        QPixmap pix(path);

        ui->stackedWidget->setCurrentIndex(3);  // 切换到照片显示页

        ui->picture_label->setPixmap(pix);
    });

    // 点击播放视频
    connect(ui->video_listWidget, &QListWidget::itemClicked, this, [=](QListWidgetItem *item){
        vdec_init(1920, 1080);
        QString path = item->data(Qt::UserRole).toString();  // 取出存储的完整路径
        emit WidgetToStartDecVideo(path);
    });

    connect(this, &Widget::WidgetToStartCapture, cameracap, &CameraCapture::startCapture);
    connect(this, &Widget::WidgetToStopCapture, cameracap, &CameraCapture::stopCapture);
    connect(this, &Widget::WidgetToStartDecVideo, videodecsend, &VideoDecSend::do_send_frame);
    connect(this, &Widget::WidgetToStartDecVideo, videodecget, &VideoDecGet::do_get_frame);

    connect(keycontrolThread, &QThread::started, keycontrol, &KeyControl::key_query);
    connect(cameracapThread, &QThread::started, cameracap, &CameraCapture::startCapture);
    // 开启线程
//    cameracapThread->start();
    savevideoThread->start();
    keycontrolThread->start();
    videodecsendThread->start();
    videodecgetThread->start();

}

Widget::~Widget()
{
    delete ui;
}


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
                      cv::Scalar(255, 0, 0),
                      cv::FILLED);

        // 绘制白色文字
        cv::putText(img, text, textOrg, fontFace, fontScale, cv::Scalar(220, 220, 220), thickness);

        QImage image(img.data, img.cols, img.rows, QImage::Format_RGB888);
        ui->video_label_3->setPixmap(QPixmap::fromImage(image));

    } else {  // 非录像下，显示系统时间
        QString timeStr = QDateTime::currentDateTime().toString("HH'h'mm'm'");
        cv::putText(img, timeStr.toStdString(),
                    cv::Point(20, 40),
                    cv::FONT_HERSHEY_SIMPLEX, 1,
                    cv::Scalar(220,220,220), 2);

        QImage image(img.data, img.cols, img.rows, QImage::Format_RGB888);
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

