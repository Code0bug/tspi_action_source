#ifndef IMGPROCESS_H
#define IMGPROCESS_H

#include <QObject>
#include <opencv2/opencv.hpp>
//#include <opencv/cv.hpp>
#include "yolov5.h"
#include "image_drawing.h"

class ImgProcess : public QObject
{
    Q_OBJECT
public:
    ImgProcess();


public slots:
    void doProcess(image_buffer_t src_image);


signals:
    void proFinished(cv::Mat img);

private:
    rknn_app_context_t rknn_app_ctx;
};

#endif // IMGPROCESS_H
