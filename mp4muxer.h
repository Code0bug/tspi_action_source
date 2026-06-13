#ifndef MP4MUXER_H
#define MP4MUXER_H

#include <QObject>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QVector>
#include <cstdint>
#include "rk_mpi.h"

struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct SwrContext;

class Mp4Muxer : public QObject
{
    Q_OBJECT
public:
    explicit Mp4Muxer(QObject *parent = nullptr);
    ~Mp4Muxer();
public slots:
    bool initMp4Mux(const QString &filename);
    void closeMp4Mux();
    int writeVideoToMp4(VENC_STREAM_S *pstStream);
    int writeAudioToMp4(QByteArray pcmData, uint64_t captureTsUs);

private:
    int addMp4Stream();
    int addSpsPpsAndWriteHeader();
    int initAudioEncoder();
    int writeAudioFrame(AVFrame *frame);

    AVFormatContext *m_outFmt = nullptr;
    AVCodecContext *m_audioEncCtx = nullptr;
    SwrContext *m_swrCtx = nullptr;
    AVFrame *m_audioFrame = nullptr;
    int m_videoStreamIndex = -1;
    int m_audioStreamIndex = -1;
    bool m_headerWritten = false;
    bool m_videoPtsInited = false;
    bool m_audioPtsInited = false;
    uint64_t m_videoPtsBaseUs = 0;
    uint64_t m_audioPtsBaseUs = 0; 
    int64_t m_lastVideoPtsUs = -1; 
    QByteArray m_sps;
    QByteArray m_pps;
};

#endif // MP4MUXER_H
