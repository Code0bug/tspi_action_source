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
    bool initMp4Mux(const QString filename);
    void closeMp4Mux();
    void do_save_video(cv::Mat buf,  FILE* fp);

signals:
    void sigVideoEncoded(const QByteArray &data, uint64_t ptsUs, bool keyFrame);

private:
    int addMp4Stream();
    int addSpsPpsAndWriteHeader(uint8_t *buf, uint32_t size);
    int writeVideoToMp4(VENC_STREAM_S *pstStream);

    VENC_STREAM_S stFrame;	 	    // 编码码流结构体，用于存储读取到的编码流
    VIDEO_FRAME_INFO_S h264_frame;  // 编码帧信息结构体
    unsigned char *data;  // 指向缓存块的指针

    // MP4
    AVFormatContext *m_outFmt = nullptr;
    int m_videoStreamIndex = -1;
    int m_audioStreamIndex = -1;
    bool m_headerWritten = false;
    int m_firstSpsPpsFound = 0; // 0:none, 1:one of SPS/PPS, 2:both
    bool m_videoPtsInited = false;
    uint64_t m_videoPtsBaseUs = 0;
    QByteArray m_sps;
    QByteArray m_pps;

};

#endif // SAVEVIDEO_H
