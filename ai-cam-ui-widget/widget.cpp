#include "widget.h"
#include "ui_widget.h"

uint8_t camStatus = 0;

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

    // 图像处理线程
    imgproThread = new QThread();
    ImgProcess* imgpro = new ImgProcess();
    imgpro->moveToThread(imgproThread);

    // 连接信号
    connect(cameracap, &CameraCapture::capFinishedToRKNN, imgpro, &ImgProcess::doProcess);
    connect(imgpro, &ImgProcess::proFinished, this, &Widget::doShow);
    connect(cameracap, &CameraCapture::capFinishedToUI, this, &Widget::doShow);

    connect(ui->open, &QPushButton::clicked, cameracap, &CameraCapture::startCapture); // 按下"打开摄像头",开启视频捕获并显示
    connect(ui->close, &QPushButton::clicked, cameracap, &CameraCapture::stopCapture, Qt::DirectConnection);
    connect(ui->start_detect, &QPushButton::clicked, cameracap, &CameraCapture::startDetect, Qt::DirectConnection);
    connect(ui->stop_detect, &QPushButton::clicked, cameracap, &CameraCapture::stopDetect, Qt::DirectConnection);


    // 开启线程
    cameracapThread->start();
    imgproThread->start();

}

Widget::~Widget()
{
    delete ui;
}


void Widget::doShow(cv::Mat img)
{
    QImage image(img.data, 640, 640, QImage::Format_RGB888);


    ui->video->setPixmap(QPixmap::fromImage(image));
}


