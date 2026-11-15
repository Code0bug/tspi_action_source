#ifndef MEDIA_SERVICE_MP4_MUXER_H
#define MEDIA_SERVICE_MP4_MUXER_H

#include "video_encoder.h"

#include <cstdint>
#include <string>
#include <vector>

struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct SwrContext;

class Mp4Muxer {
public:
    Mp4Muxer();
    ~Mp4Muxer();

    Mp4Muxer(const Mp4Muxer &) = delete;
    Mp4Muxer &operator=(const Mp4Muxer &) = delete;

    bool initMp4Mux(const std::string &filename);
    void closeMp4Mux();
    int writeVideoToMp4(const EncodedPacket *pstStream);
    int writeAudioToMp4(const uint8_t *pcmData, int pcmSize, uint64_t captureTsUs);

private:
    int addMp4Stream();
    int addSpsPpsAndWriteHeader();
    int initAudioEncoder();
    int writeAudioFrame(AVFrame *frame);

    AVFormatContext *m_outFmt;
    AVCodecContext *m_audioEncCtx;
    SwrContext *m_swrCtx;
    AVFrame *m_audioFrame;
    int m_videoStreamIndex;
    int m_audioStreamIndex;
    bool m_headerWritten;
    bool m_videoPtsInited;
    bool m_audioPtsInited;
    uint64_t m_videoPtsBaseUs;
    uint64_t m_audioPtsBaseUs;
    int64_t m_lastVideoPtsUs;
    std::vector<uint8_t> m_sps;
    std::vector<uint8_t> m_pps;
};

#endif
