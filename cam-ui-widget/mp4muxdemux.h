#ifndef MP4MUXDEMUX_H
#define MP4MUXDEMUX_H

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

class Mp4MuxDemux : public QObject
{
    Q_OBJECT
public:
    explicit Mp4MuxDemux(QObject *parent = nullptr);
    ~Mp4MuxDemux();
    static int demuxMp4ToH264(const QString &mp4Path,
                              const QString &h264OutPath,
                              const QString &audioOutPath = QString());

public slots:
    bool initMp4Mux(const QString &filename);
    void closeMp4Mux();
    int writeVideoToMp4(VENC_STREAM_S *pstStream);
    int writeAudioToMp4(QByteArray pcmData, uint64_t captureTsUs);

private:
    struct AudioChunk {
        QByteArray pcm;
        uint64_t tsUs;
    };

    int addMp4Stream();
    int addSpsPpsAndWriteHeader();
    int initAudioEncoder();
    void releaseAudioEncoder();
    void flushPendingAudio();
    int processAudioChunk(const QByteArray &pcmData, uint64_t captureTsUs);
    int writeAudioFrame(AVFrame *frame);
    void flushAudioEncoder();
    void ensureSyncBase(uint64_t tsUs);

    AVFormatContext *m_outFmt = nullptr;
    AVCodecContext *m_audioEncCtx = nullptr;
    SwrContext *m_swrCtx = nullptr;
    AVFrame *m_audioFrame = nullptr;
    int m_videoStreamIndex = -1;
    int m_audioStreamIndex = -1;
    bool m_headerWritten = false;
    bool m_videoPtsInited = false;
    bool m_audioPtsInited = false;
    bool m_syncBaseInited = false;
    uint64_t m_videoPtsBaseUs = 0;
    uint64_t m_syncBaseUs = 0;
    int64_t m_audioNextPts = 0;      // 单位：audio stream time_base（samples）
    int m_audioInputChannels = 1;
    int m_audioInputBytesPerSample = 2; // s16 = 2 bytes
    QByteArray m_audioPcmCache;
    QVector<AudioChunk> m_pendingAudioChunks;
    QByteArray m_sps;
    QByteArray m_pps;
};

#endif // MP4MUXDEMUX_H
