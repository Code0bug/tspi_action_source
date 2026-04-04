#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QThread>
#include <QStackedWidget>
#include <QFileSystemModel>
#include <QScrollBar>
#include <QLayout>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QListWidgetItem>

#include "cameracapture.h"
#include "savevideo.h"
#include "savepicture.h"
#include "keycontrol.h"
#include "videodecget.h"
#include "videodecsend.h"
#include "imureader.h"
#include "rk_mpi.h"
#include "mp4muxdemux.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

public slots:
    void doShow(cv::Mat img);
    void switchToPage1();
    void switchToPage2();
    void switchToPage0();
    void switchToPage5();
    void updatePicture(QString path);
    void updateVideo(QString path, cv::Mat first_frame);
    void doShowVideo(cv::Mat img);
    void stopVideo();
    void showNextPhoto();
    void showPrevPhoto();
    void PicListItemClicked(QListWidgetItem *item);
    void VideoListItemClicked(QListWidgetItem *item);
//    void stop_imu_record();
//    void start_imu_record();


signals:
    void WidgetToStartCapture();
    void WidgetToStopCapture();
    void WidgetToStartDecVideo(QString path);


private slots:


private:
    Ui::Widget *ui;
    QThread *cameracapThread;
    QThread *savevideoThread;
    QThread *savepictureThread;
    QThread *keycontrolThread;
    QThread *playvideoThread;
    QThread *videodecsendThread;
    QThread *videodecgetThread;
    QThread *imuThread;
    QThread *mp4muxThread;

    uint8_t video_flag;
    QDateTime recordStartTime;

    int picture_current_index; // 当前显示图片的索引

//    QTimer IMUTimer;



};


#endif // WIDGET_H
