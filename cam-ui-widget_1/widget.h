#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QThread>
#include <QStackedWidget>
#include <QFileSystemModel>
#include <QScrollBar>

#include "cameracapture.h"
#include "savevideo.h"
#include "keycontrol.h"
#include "videodecget.h"
#include "videodecsend.h"
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
    void updatePicture(QString path);
    void updateVideo(QString path);
    void doShowVideo(cv::Mat img);


signals:
    void WidgetToStartCapture();
    void WidgetToStopCapture();
    void WidgetToStartDecVideo(QString path);


private:
    Ui::Widget *ui;
    QThread *cameracapThread;
    QThread *savevideoThread;
    QThread *keycontrolThread;
    QThread *playvideoThread;
    QThread *videodecsendThread;
    QThread *videodecgetThread;



};


#endif // WIDGET_H
