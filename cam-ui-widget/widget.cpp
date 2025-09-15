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

    qRegisterMetaType<image_buffer_t>("image_buffer_t");
    qRegisterMetaType<cv::Mat>("cv::Mat");

    // 视频采集线程
    cameracapThread = new QThread();
    CameraCapture* cameracap = new CameraCapture();
    cameracap->moveToThread(cameracapThread);

    // 连接信号
    connect(cameracap, &CameraCapture::capFinishedToUI, this, &Widget::doShow);

    connect(ui->open, &QPushButton::clicked, cameracap, &CameraCapture::startCapture); // 按下"打开摄像头",开启视频捕获并显示
    connect(ui->close, &QPushButton::clicked, cameracap, &CameraCapture::stopCapture, Qt::DirectConnection);
    connect(ui->photo, &QPushButton::clicked, cameracap, &CameraCapture::takePhoto, Qt::DirectConnection);

    // 开启线程
    cameracapThread->start();
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


    ui->video->setPixmap(QPixmap::fromImage(image));
}


