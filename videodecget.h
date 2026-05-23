#ifndef VIDEODECGET_H
#define VIDEODECGET_H

#include <QObject>
#include <QWidget>
#include "rk_mpi.h"
#include <opencv2/opencv.hpp>
#include "im2d.h"
#include "rga.h"
#include "RgaUtils.h"
#include "videodecsend.h"

class VideoDecGet : public QObject
{
    Q_OBJECT
public:
    explicit VideoDecGet(QObject *parent = nullptr);

signals:
    void VideodecToUI(cv::Mat img);

public slots:
    void do_get_frame();
    void do_stop_get_frame();

private:
    uint8_t stop_get_frame;

};

#endif // VIDEODECGET_H
