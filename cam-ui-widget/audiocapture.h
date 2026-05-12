#ifndef AUDIOCAPTURE_H
#define AUDIOCAPTURE_H

#include <QObject>
#include <QByteArray>
#include <cstdint>

#include "rk_mpi.h"
#include "rk_mpi_ai.h"
#include "rk_mpi_ao.h"

struct AVCodecContext;
struct AVFrame;
struct SwrContext;

typedef struct AudioCaptureConfig {
    AUDIO_DEV devId;
    AI_CHN chnId;
    RK_S32 deviceChannel;
    RK_S32 deviceSampleRate;
    RK_S32 outChannel;
    RK_S32 outSampleRate;
    RK_S32 outBitWidth;
    RK_S32 periodCount;
    RK_S32 periodSize;
    const char *cardName;
} AudioCaptureConfig;

// 在外部可直接修改该配置（例如 main/widget 中）
extern AudioCaptureConfig gAudioCaptureConfig;

class AudioManager : public QObject
{
    Q_OBJECT

public:
    AudioManager();
    ~AudioManager();

public slots:
    void startCapture();
    void stopCapture();
    void startPlayback(int codecId, int sampleRate, int channels, QByteArray extradata);
    void playEncodedAudioPacket(QByteArray packetData, qint64 pts);
    void stopPlayback();

signals:
    void capFinishedToAudio(QByteArray pcmData, uint64_t captureTsUs);

private:
    RK_S32 openDevice();
    RK_S32 initChannel();
    RK_S32 deinitChannel();
    RK_S32 openPlaybackDevice(int sampleRate, int channels);
    RK_S32 initPlaybackChannel(int sampleRate);
    RK_S32 deinitPlaybackChannel();
    int decodeAndPlayPacket(const uint8_t *data, int size, qint64 pts);
    int playDecodedFrame(AVFrame *frame);
    int sendPcmToAo(const uint8_t *data, int size, uint64_t pts);
    void releasePlaybackDecoder();
    AUDIO_SOUND_MODE_E findSoundMode(RK_S32 ch);
    AUDIO_BIT_WIDTH_E findBitWidth(RK_S32 bit);

    uint8_t isRunning;
    uint8_t isPlaying;
    AUDIO_DEV aoDevId;
    AO_CHN aoChnId;
    int playbackSampleRate;
    int playbackChannels;
    uint64_t playbackPts;
    AVCodecContext *audioDecCtx;
    SwrContext *audioSwrCtx;
    AVFrame *audioFrame;
};

using AudioCapture = AudioManager;

#endif // AUDIOCAPTURE_H
