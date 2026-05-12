#ifndef VIDEODECSEND_H
#define VIDEODECSEND_H

#include <QObject>
#include <QWidget>
#include <QByteArray>
#include "rk_mpi.h"

class VideoDecSend : public QObject
{
    Q_OBJECT
public:
    explicit VideoDecSend(QObject *parent = nullptr);

signals:
    void audioStreamStarted(int codecId, int sampleRate, int channels, QByteArray extradata);
    void audioPacketReady(QByteArray packetData, qint64 pts);
    void audioStreamFinished();

public slots:
    void do_send_frame(QString path);
    void do_stop_send_frame();

private:
//    FILE* fp; // 读取视频文件
    int send_stream_data(const uint8_t *src, RK_U32 len, bool eos, bool endOfFrame, uint64_t ptsUs = 0);
    int do_send_mp4_frame(const QString &path);
    uint8_t stop_send_frame;

};

extern std::atomic<bool> ReachEOS;

#endif // VIDEODECSEND_H
