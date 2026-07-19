#ifndef VIDEOENCODER_H
#define VIDEOENCODER_H

#include <QObject>
#include <QByteArray>
#include <QList>
#include <QString>
#include <cstdint>
#include "rk_mpi.h"

#define encWidth 1920
#define encHeight 1080


class VideoEncoder : public QObject
{
    Q_OBJECT
public:
    VideoEncoder();
    ~VideoEncoder();

public slots:
    void do_save_video(void* mbBlk, uint64_t pts);

signals:
    void sigVideo2Mp4Mux(VENC_STREAM_S *pstStream);
    void sigReleaseViFrame(void* mbBlk);

private:
    VENC_STREAM_S stFrame;	 	    // 编码码流结构体，用于存储读取到的编码流
    VIDEO_FRAME_INFO_S h264_frame;  // 编码帧信息结构体

};

#endif // VIDEOENCODER_H
