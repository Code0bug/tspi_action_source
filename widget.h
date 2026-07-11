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

#include "cameramanager.h"
#include "videoencoder.h"
#include "savepicture.h"
#include "keycontrol.h"
#include "videodecoder.h"
#include "mp4demuxer.h"
#include "imureader.h"
#include "rk_mpi.h"
#include "mp4muxer.h"
#include "audiomanager.h"

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
    void doShow(int fd, int w, int h, void* mbBlk, uint64_t pts);
    void switchToPage1();
    void switchToPage2();
    void switchToPage0();
    void switchToPage5();
    void updatePicture(QString path);
    void updateVideo(QString path, cv::Mat first_frame);
    void doShowVideo(int fd, int w, int h, void* mbBlk, int stride, int virHeight);
    void stopVideo();
    void showNextPhoto();
    void showPrevPhoto();
    void PicListItemClicked(QListWidgetItem *item);
    void VideoListItemClicked(QListWidgetItem *item);


signals:
    void WidgetToStartCapture();
    void WidgetToStopCapture();
    void WidgetToStartDecVideo(QString path);


private slots:


private:
    Ui::Widget *ui;
    QThread *cameraManagerThread;
    QThread *videoEncoderThread;
    QThread *savePictureThread;
    QThread *keyControlThread;
    QThread *mp4DemuxerThread;
    QThread *videoDecoderThread;
    QThread *imuReaderThread;
    QThread *mp4MuxerThread;
    QThread *audioManagerThread;

    uint8_t video_flag;
    QDateTime recordStartTime;

    int picture_current_index; // 当前显示图片的索引

//    QTimer IMUTimer;



};


#endif // WIDGET_H
