#ifndef CAMERAMANAGER_H
#define CAMERAMANAGER_H

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QString>

#include <atomic>
#include <mutex>
#include <iostream>
#include "rk_mpi.h"
#include <opencv2/opencv.hpp>
#include "widget.h"

#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <QHash>

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
    void releaseViFrame(void* mbBlk);

private:
    cv::Mat stabilizeFrame(const cv::Mat& frame);
    std::atomic<bool> isRunning;
    uint8_t photo_flag;
    uint8_t video_flag;
    uint8_t first_video_frame;

    // 待释放的 VI 帧：MB_BLK → {帧信息, 引用计数}
    // 录像时 VI 帧有两个消费者：
    //   1) VideoEncoder（videoEncoderThread 异步处理）
    //   2) VideoGLWidget（GUI 线程渲染）
    // 都把对应的 mbBlk 在 sigReleaseViFrame 里发回 CameraManager::releaseViFrame,
    // refCount 归零才真正 RK_MPI_VI_ReleaseChnFrame。
    struct PendingFrame {
        VIDEO_FRAME_INFO_S frame;
        int refCount;
    };
    QHash<void*, PendingFrame> pendingViFrames_;
    std::mutex pendingMtx_;


signals:
    // 预览改传 DMA-BUF 的 fd + 几何 + MB_BLK（MB_BLK 用于 UI 回发释放）
    void capFinishedToUI(int fd, int w, int h, void* mbBlk, uint64_t pts);
    void capFinishedToVideo(void* mbBlk, uint64_t pts);
//    void photoFinishedToUpdate(QString path);
    void videoFinishedToUpdate(QString path, cv::Mat first_frame);
    void videoFinishedToIMURecord(QString path);
    void photoFinishedToSave(QString path, cv::Mat frame);
    void videoFinishedToMP4();
    void sigVideoMp4Init(QString path);



};

#endif // CAMERAMANAGER_H
