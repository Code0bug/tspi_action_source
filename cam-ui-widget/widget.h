#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QThread>
#include <QStackedWidget>

#include "cameracapture.h"
#include "savevideo.h"
#include "keycontrol.h"

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


private slots:


private:
    Ui::Widget *ui;
    QThread *cameracapThread;
    QThread *savevideoThread;
    QThread *keycontrolThread;


};

extern uint8_t camStatus;

#endif // WIDGET_H
