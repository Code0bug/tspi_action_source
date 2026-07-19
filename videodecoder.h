#ifndef VIDEODECODER_H
#define VIDEODECODER_H

#include <QObject>
#include <QWidget>
#include <atomic>
#include "rk_mpi.h"
#include <opencv2/opencv.hpp>
#include <QHash>

class VideoDecoder : public QObject
{
    Q_OBJECT
public:
    explicit VideoDecoder(QObject *parent = nullptr);
    ~VideoDecoder() override;

signals:
    void VideodecToUI(int fd, int w, int h, void* mbBlk, int stride, int virHeight);

public slots:
    void do_get_frame();
    void do_stop_get_frame();
    void releaseVdecFrame(void* mbBlk);

private:
    std::atomic<bool> stop_get_frame;
    std::atomic<bool> vdecReady;
    QHash<void*, VIDEO_FRAME_INFO_S> pendingVdecFrames_;
    void destroyVdec();
    void reinitVdec();
};

#endif // VIDEODECODER_H
