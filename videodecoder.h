#ifndef VIDEODECODER_H
#define VIDEODECODER_H

#include <QObject>
#include <QWidget>
#include "rk_mpi.h"
#include <opencv2/opencv.hpp>
#include "im2d.h"
#include "rga.h"
#include "RgaUtils.h"
#include "mp4demuxer.h"

class VideoDecoder : public QObject
{
    Q_OBJECT
public:
    explicit VideoDecoder(QObject *parent = nullptr);

signals:
    void VideodecToUI(cv::Mat img);

public slots:
    void do_get_frame();
    void do_stop_get_frame();

private:
    uint8_t stop_get_frame;

};

#endif // VIDEODECODER_H
