#ifndef VIDEODECSEND_H
#define VIDEODECSEND_H

#include <QObject>
#include <QWidget>
#include "rk_mpi.h"

class VideoDecSend : public QObject
{
    Q_OBJECT
public:
    explicit VideoDecSend(QObject *parent = nullptr);

signals:

public slots:
    void do_send_frame(QString path);
    void do_stop_send_frame();

private:
//    FILE* fp; // 读取视频文件
    uint8_t stop_send_frame;

};

extern std::atomic<bool> ReachEOS;

#endif // VIDEODECSEND_H
