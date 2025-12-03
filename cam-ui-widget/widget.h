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

#include "cameracapture.h"
#include "savevideo.h"
#include "keycontrol.h"
#include "videodecget.h"
#include "videodecsend.h"
#include "photomanagement.h"
#include "rk_mpi.h"

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


signals:
    void WidgetToStartCapture();
    void WidgetToStopCapture();
    void WidgetToStartDecVideo(QString path);


private slots:


private:
    Ui::Widget *ui;
    QThread *cameracapThread;
    QThread *savevideoThread;
    QThread *keycontrolThread;
    QThread *playvideoThread;
    QThread *videodecsendThread;
    QThread *videodecgetThread;

    uint8_t video_flag;
    QDateTime recordStartTime;

    int picture_current_index; // 当前显示图片的索引



};


#endif // WIDGET_H
