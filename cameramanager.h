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

    // 零拷贝录像：BGR 输出 DMA 双缓冲
    MB_BLK bgr_blk_[2];
    RK_S32 bgr_fd_[2];
    int write_idx_;

signals:
    void capFinishedToUI(cv::Mat imgbuf);
    void capFinishedToVideo(void* mbBlk, uint64_t pts);
//    void photoFinishedToUpdate(QString path);
    void videoFinishedToUpdate(QString path, cv::Mat first_frame);
    void videoFinishedToIMURecord(QString path);
    void photoFinishedToSave(QString path, cv::Mat frame);
    void videoFinishedToMP4();
    void sigVideoMp4Init(QString path);



};

#endif // CAMERAMANAGER_H
