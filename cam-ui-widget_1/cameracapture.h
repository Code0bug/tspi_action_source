#ifndef CAMERACAPTURE_H
#define CAMERACAPTURE_H

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QString>

#include <iostream>
#include "rk_mpi.h"
#include <opencv2/opencv.hpp>
#include "widget.h"


#include "im2d.h"
#include "rga.h"
#include "RgaUtils.h"

#include "rockitmpi.h"


class CameraCapture : public QObject
{
    Q_OBJECT
public:
    CameraCapture();
    ~CameraCapture();

public slots:
    void startCapture();
    void stopCapture();
    void startVideo();
    void stopVideo();
    void takePhoto();

private:
    uint8_t isRunning;
    uint8_t photo_flag;
    uint8_t video_flag;
    FILE* video_fp;

signals:
    void capFinishedToUI(cv::Mat imgbuf);
    void capFinishedToVideo(cv::Mat buf, FILE* fp);
    void photoFinishedToUpdate(QString path);
    void videoFinishedToUpdate(QString path);



};

#endif // CAMERACAPTURE_H
