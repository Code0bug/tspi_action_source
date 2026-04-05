#ifndef MP4MUXDEMUX_H
#define MP4MUXDEMUX_H

#include <QObject>
#include <QByteArray>
#include <QList>
#include <QString>
#include <cstdint>
#include "rk_mpi.h"

struct AVFormatContext;

class Mp4MuxDemux : public QObject
{
    Q_OBJECT
public:
    explicit Mp4MuxDemux(QObject *parent = nullptr);
    ~Mp4MuxDemux();

public slots:
    bool initMp4Mux(const QString &filename);
    void closeMp4Mux();
    int writeVideoToMp4(VENC_STREAM_S *pstStream);

private:
    int addMp4Stream();
    int addSpsPpsAndWriteHeader();

    AVFormatContext *m_outFmt = nullptr;
    int m_videoStreamIndex = -1;
    int m_audioStreamIndex = -1;
    bool m_headerWritten = false;
    bool m_videoPtsInited = false;
    uint64_t m_videoPtsBaseUs = 0;
    QByteArray m_sps;
    QByteArray m_pps;
};

#endif // MP4MUXDEMUX_H
