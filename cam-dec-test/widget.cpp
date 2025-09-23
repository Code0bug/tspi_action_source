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

    videosendThread = new QThread();
    videogetThread = new QThread();
    VideoDecGet* videoget = new VideoDecGet();
    VideoDecSend* videosend = new VideoDecSend();
    videoget->moveToThread(videogetThread);
    videosend->moveToThread(videosendThread);


    // rkmpi init
    if (RK_MPI_SYS_Init() != RK_SUCCESS) {
        RK_LOGE("rk mpi sys init fail!");
        // return -1;
    }

    vdec_init(1920, 1080);

    connect(videosendThread, &QThread::started, videosend, &VideoDecSend::do_send_frame);
    connect(videogetThread, &QThread::started, videoget, &VideoDecGet::do_get_frame);
    connect(videoget, &VideoDecGet::PlayVideoToUI, this, &Widget::doShow);

    videosendThread->start();
    videogetThread->start();

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





