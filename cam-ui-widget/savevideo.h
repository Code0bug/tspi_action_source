#ifndef SAVEVIDEO_H
#define SAVEVIDEO_H

#include <QObject>
#include <QByteArray>
#include <QList>
#include <QString>
#include <cstdint>
#include "rk_mpi.h"
#include <opencv2/opencv.hpp>

struct AVFormatContext;
struct AVStream;

#define encWidth 1920
#define encHeight 1080


class SaveVideo : public QObject
{
    Q_OBJECT
public:
    SaveVideo();
    ~SaveVideo();

public slots:
    void do_save_video(cv::Mat buf, FILE* fp, uint64_t pts);

signals:
    void sigVideo2Mp4Mux(VENC_STREAM_S *pstStream);

private:
    VENC_STREAM_S stFrame;	 	    // 编码码流结构体，用于存储读取到的编码流
    VIDEO_FRAME_INFO_S h264_frame;  // 编码帧信息结构体
    unsigned char *data;  // 指向缓存块的指针

};

#endif // SAVEVIDEO_H
