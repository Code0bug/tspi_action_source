#ifndef MEDIA_SERVICE_MP4_MANAGER_H
#define MEDIA_SERVICE_MP4_MANAGER_H

#include "mpp_codec.h"

#include <camera_hal.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct SwrContext;

class MediaManager;

enum class MuxJobType {
    Open,
    Video,
    Audio,
    Close,
};

struct MuxJob {
    MuxJobType type;
    camera_frame_t video;
    std::vector<uint8_t> pcm;
    uint64_t pts;
    std::string path;
};

enum class DemuxJobType {
    AudioStart,   // 音频流开始，携带解码参数
    AudioPacket,  // 一包压缩音频
    AudioEnd,     // 音频流结束或停止
};

struct DemuxJob {
    DemuxJobType type;
    // AudioStart
    int codec_id;
    int sample_rate;
    int channels;
    std::vector<uint8_t> extradata;
    // AudioPacket
    std::vector<uint8_t> data;
    int64_t pts;
};

class Mp4Manager {
public:
    explicit Mp4Manager(MediaManager &media);
    ~Mp4Manager();

    Mp4Manager(const Mp4Manager &) = delete;
    Mp4Manager &operator=(const Mp4Manager &) = delete;

    void startMux();
    void stopMux();
    void stop();
    void muxRun();
    void startDemux(const std::string &path);
    void demuxRun(std::string path);
    void stopDemux();

    bool initMp4Mux(const std::string &filename);
    void closeMp4Mux();
    int writeVideoToMp4(const EncodedPacket *pstStream);
    int writeAudioToMp4(const uint8_t *pcmData, int pcmSize, uint64_t captureTsUs);

private:
    int addMp4Stream();
    int addSpsPpsAndWriteHeader();
    int initAudioEncoder();
    int writeAudioFrame(AVFrame *frame);
    int demuxMp4(const std::string &path);
    int sendVideoStream(const uint8_t *src, uint32_t len, bool eos, uint64_t ptsUs = 0);

    MediaManager &media_;

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
    std::thread muxer_thread;
    std::atomic<bool> stop_demux_{false};
};

#endif
