#include "widget.h"
#include "ui_widget.h"

uint8_t camStatus = 0;
// 用于fps显示
static char fps_text[16];
static float fps = 0;

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
        "   background-color: rgba(255, 255, 255, 150);"
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
        "   background-color: rgba(255, 255, 255, 150);"
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
    ui->switch_button_2->setFixedSize(80, 80);

    // 设置样式表
    ui->switch_button_2->setStyleSheet(
        "QPushButton {"
        "   background-color: rgba(255, 255, 255, 150);"
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

    // 视频采集线程
    cameracapThread = new QThread();
    CameraCapture* cameracap = new CameraCapture();
    cameracap->moveToThread(cameracapThread);

    savevideoThread = new QThread();
    SaveVideo* savevideo = new SaveVideo();
    savevideo->moveToThread(savevideoThread);

    keycontrolThread = new QThread();
    KeyControl* keycontrol = new KeyControl();
    keycontrol->moveToThread(keycontrolThread);

    // 连接信号
    connect(cameracap, &CameraCapture::capFinishedToUI, this, &Widget::doShow);
    connect(cameracap, &CameraCapture::capFinishedToVideo, savevideo, &SaveVideo::do_save_video);

    connect(keycontrol, &KeyControl::keyToTakephoto, cameracap, &CameraCapture::takePhoto, Qt::DirectConnection);
    connect(keycontrol, &KeyControl::keyToStartvideo, cameracap, &CameraCapture::startVideo, Qt::DirectConnection);
    connect(keycontrol, &KeyControl::keyToStopvideo, cameracap, &CameraCapture::stopVideo, Qt::DirectConnection);
    connect(ui->switch_button_0, &QPushButton::clicked, this, &Widget::switchToPage1);
    connect(ui->switch_button_1, &QPushButton::clicked, this, &Widget::switchToPage2);
    connect(ui->switch_button_2, &QPushButton::clicked, this, &Widget::switchToPage0);

    connect(keycontrolThread, &QThread::started, keycontrol, &KeyControl::key_query);
    connect(cameracapThread, &QThread::started, cameracap, &CameraCapture::startCapture);

    // 开启线程
    cameracapThread->start();
    savevideoThread->start();
    keycontrolThread->start();


}

Widget::~Widget()
{
    delete ui;
}



void Widget::doShow(cv::Mat img)
{

    // 推流
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

void Widget::switchToPage1()
{
    ui->stackedWidget->setCurrentIndex(1);
}

void Widget::switchToPage2()
{
    ui->stackedWidget->setCurrentIndex(2);
}

void Widget::switchToPage0()
{
    ui->stackedWidget->setCurrentIndex(0);
}





