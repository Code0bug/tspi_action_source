#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QThread>
#include "cameracapture.h"
#include "imgprocess.h"

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


private:
    Ui::Widget *ui;
    QThread *cameracapThread;
    QThread *imgproThread;
};

extern uint8_t camStatus;

#endif // WIDGET_H
