#ifndef MP4MUXDEMUX_H
#define MP4MUXDEMUX_H

#include <QObject>
#include <QByteArray>
#include <QList>
#include <QString>
#include <cstdint>

struct AVRational;
struct AVFormatContext;
struct AVStream;

class Mp4MuxDemux : public QObject
{
    Q_OBJECT
public:
    enum StreamType {
        StreamUnknown = -1,
        StreamVideo = 0,
        StreamAudio = 1
    };

    struct VideoConfig {
        int width = 0;
        int height = 0;
        int fps = 30;
        int timebaseNum = 1;
        int timebaseDen = 1000000; // microseconds
    };

    struct AudioConfig {
        int sampleRate = 0;
        int channels = 0;
        int bitRate = 0;
        int profile = 0;
        int codecId = 0; // AVCodecID, kept as int to avoid exposing ffmpeg headers here
    };

    struct Packet {
        StreamType type = StreamUnknown;
        QByteArray data;
        qint64 ptsUs = -1;
        qint64 dtsUs = -1;
        bool keyFrame = false;
        bool eof = false;
    };

    explicit Mp4MuxDemux(QObject *parent = nullptr);
    ~Mp4MuxDemux();

    // 打开 MP4 封装器（仅创建容器与流信息，真正写头在拿到 SPS/PPS 后执行）
    bool openForWrite(const QString &path, const VideoConfig &videoCfg);
    bool setAudioConfigForWrite(const AudioConfig &audioCfg);
    // 写入编码后视频包（输入通常是 H264 AnnexB）
    bool writeVideoPacket(const uint8_t *data, int size, qint64 ptsUs, bool keyFrame);
    // 预留音频写入接口（后续可接 AAC/其他编码音频）
    bool writeAudioPacket(const uint8_t *data, int size, qint64 ptsUs);
    void closeWrite();

    // 打开 MP4 解封装器，按包读取（视频/音频）
    bool openForRead(const QString &path);
    bool readNextPacket(Packet *outPacket);
    void closeRead();

    QString lastError() const;

public slots:
    // mux slots for future signal/slot integration
    void slotStartMux(const QString &path, int width, int height, int fps = 30);
    void slotStopMux();
    void slotWriteVideoEncoded(const QByteArray &data, qint64 ptsUs, bool keyFrame);
    void slotWriteAudioEncoded(const QByteArray &data, qint64 ptsUs);

    // demux slots for future signal/slot integration
    void slotStartDemux(const QString &path);
    void slotStopDemux();
    void slotPullNextPacket();

signals:
    void sigMuxOpened(bool ok, const QString &message);
    void sigMuxClosed();
    void sigMuxError(const QString &error);

    void sigDemuxOpened(bool ok, const QString &message);
    void sigDemuxClosed();
    void sigDemuxPacketReady(int streamType, const QByteArray &data,
                             qint64 ptsUs, qint64 dtsUs, bool keyFrame, bool eof);
    void sigDemuxEof();
    void sigDemuxError(const QString &error);

private:
    bool ensureWriteHeader();
    bool cacheOrWriteVideo(const uint8_t *data, int size, qint64 ptsUs, bool keyFrame);
    bool buildVideoExtradataFromAnnexB(const uint8_t *data, int size);
    void clearPendingVideo();

    bool initH264AvccInfoFromExtradata();
    bool handleDemuxPacket(struct AVPacket *pkt, Packet *outPacket);
    QByteArray convertH264AvccToAnnexB(const uint8_t *data, int size, bool keyFrame) const;

    qint64 rescaleToStreamTimebase(qint64 ptsUs, const AVRational &tb) const;
    qint64 rescaleToUs(qint64 ts, const AVRational &tb) const;

private:
    struct PendingVideo {
        QByteArray data;
        qint64 ptsUs = -1;
        bool keyFrame = false;
    };

    QString m_lastError;

    // mux
    AVFormatContext *m_outFmt = nullptr;
    AVStream *m_videoOutStream = nullptr;
    AVStream *m_audioOutStream = nullptr;
    VideoConfig m_videoCfg;
    AudioConfig m_audioCfg;
    bool m_audioConfigured = false;
    bool m_headerWritten = false;
    bool m_writeOpened = false;

    QByteArray m_sps;
    QByteArray m_pps;
    QList<PendingVideo> m_pendingVideo;

    // demux
    AVFormatContext *m_inFmt = nullptr;
    int m_videoIndex = -1;
    int m_audioIndex = -1;
    int m_h264NalLenSize = 4;
    QByteArray m_h264AnnexbExtradata;
    bool m_readOpened = false;
};

#endif // MP4MUXDEMUX_H
