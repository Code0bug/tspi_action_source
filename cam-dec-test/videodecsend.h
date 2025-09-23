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
    void do_send_frame();

};

extern std::atomic<bool> ReachEOS;

#endif // VIDEODECSEND_H
