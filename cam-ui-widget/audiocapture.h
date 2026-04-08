#ifndef AUDIOCAPTURE_H
#define AUDIOCAPTURE_H

#include <QObject>
#include <QByteArray>
#include <cstdint>

#include "rk_mpi.h"
#include "rk_mpi_ai.h"

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

class AudioCapture : public QObject
{
    Q_OBJECT

public:
    AudioCapture();
    ~AudioCapture();

public slots:
    void startCapture();
    void stopCapture();

signals:
    void capFinishedToAudio(QByteArray pcmData, uint64_t captureTsUs);

private:
    RK_S32 openDevice();
    RK_S32 initChannel();
    RK_S32 deinitChannel();
    AUDIO_SOUND_MODE_E findSoundMode(RK_S32 ch);
    AUDIO_BIT_WIDTH_E findBitWidth(RK_S32 bit);

    uint8_t isRunning;
};

#endif // AUDIOCAPTURE_H
