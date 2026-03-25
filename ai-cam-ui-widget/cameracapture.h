#ifndef CAMERACAPTURE_H
#define CAMERACAPTURE_H

#include <QObject>
#include <QTimer>
#include <iostream>
#include "rk_mpi.h"
#include <opencv2/opencv.hpp>
//#include <opencv/cv.hpp>
#include "image_utils.h"
#include "widget.h"

class CameraCapture : public QObject
{
    Q_OBJECT
public:
    CameraCapture();

public slots:
    void startCapture();
    void stopCapture();
    void startDetect();
    void stopDetect();

private:
    uint8_t isRunning;
    uint8_t detect_status;

signals:
    void capFinishedToRKNN(image_buffer_t imgbuf);
    void capFinishedToUI(cv::Mat imgbuf);



};

#endif // CAMERACAPTURE_H
