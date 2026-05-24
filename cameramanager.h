#ifndef CAMERAMANAGER_H
#define CAMERAMANAGER_H

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QString>

#include <iostream>
#include "rk_mpi.h"
#include <opencv2/opencv.hpp>
#include "widget.h"

#include <linux/videodev2.h>
#include <sys/ioctl.h>

#include "im2d.h"
#include "rga.h"
#include "RgaUtils.h"

//#include "rk_aiq_user_api2_imgproc.h"
//#include "rk_aiq_user_api2_sysctl.h"



class CameraManager : public QObject
{
    Q_OBJECT
public:
    CameraManager();
    ~CameraManager();

public slots:
    void startCapture();
    void stopCapture();
    void startVideo();
    void stopVideo();
    void takePhoto();

private:
    cv::Mat stabilizeFrame(const cv::Mat& frame);
    uint8_t isRunning;
    uint8_t photo_flag;
    uint8_t video_flag;
    uint8_t first_video_frame;
    FILE* video_fp;

signals:
    void capFinishedToUI(cv::Mat imgbuf);
    void capFinishedToVideo(cv::Mat buf, FILE* fp, uint64_t pts);
//    void photoFinishedToUpdate(QString path);
    void videoFinishedToUpdate(QString path, cv::Mat first_frame);
    void videoFinishedToIMURecord(QString path);
    void photoFinishedToSave(QString path, cv::Mat frame);
    void videoFinishedToMP4();
    void sigVideoMp4Init(QString path);



};

#endif // CAMERAMANAGER_H
