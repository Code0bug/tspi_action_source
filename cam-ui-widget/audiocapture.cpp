#include "audiocapture.h"

#include <cstring>
#include <climits>

AudioCaptureConfig gAudioCaptureConfig = {
    0,      // devId
    0,      // chnId
    2,      // deviceChannel
    16000,  // deviceSampleRate
    2,      // outChannel
    16000,  // outSampleRate
    16,     // outBitWidth
    4,      // periodCount
    1024,   // periodSize
    "default" // cardName
};

AudioCapture::AudioCapture()
    : isRunning(0)
{
    if (RK_MPI_SYS_Init() != RK_SUCCESS) {
        RK_LOGE("rk mpi sys init fail!");
    }
}

AudioCapture::~AudioCapture()
{
    if (isRunning) {
        stopCapture();
    }

    deinitChannel();
    RK_MPI_SYS_Exit();
}

void AudioCapture::startCapture()
{
    if (isRunning) {
        return;
    }

    if (openDevice() != RK_SUCCESS) {
        RK_LOGE("audio open device fail");
        return;
    }

    if (initChannel() != RK_SUCCESS) {
        RK_LOGE("audio init channel fail");
        return;
    }

    isRunning = 1;
    while (isRunning) {
        AUDIO_FRAME_S frame;
        RK_S32 ret = RK_MPI_AI_GetFrame(gAudioCaptureConfig.devId, gAudioCaptureConfig.chnId, &frame, RK_NULL, -1);
        if (ret != RK_SUCCESS) {
            continue;
        }

        // printf("Audio PTS = %ld\r\n", frame.u64TimeStamp);

        void *data = RK_MPI_MB_Handle2VirAddr(frame.pMbBlk);
        RK_U64 len = RK_MPI_MB_GetLength(frame.pMbBlk);
        if (data != nullptr && len > 0) {
            int copyLen = (len > static_cast<RK_U64>(INT_MAX)) ? INT_MAX : static_cast<int>(len);
            QByteArray pcm(reinterpret_cast<const char *>(data), copyLen);
            emit capFinishedToAudio(pcm, frame.u64TimeStamp);
        }

        RK_MPI_AI_ReleaseFrame(gAudioCaptureConfig.devId, gAudioCaptureConfig.chnId, &frame, RK_NULL);
    }

    deinitChannel();
}

void AudioCapture::stopCapture()
{
    if (!isRunning) {
        return;
    }

    isRunning = 0;
}

AUDIO_SOUND_MODE_E AudioCapture::findSoundMode(RK_S32 ch)
{
    switch (ch) {
    case 1:
        return AUDIO_SOUND_MODE_MONO;
    case 2:
        return AUDIO_SOUND_MODE_STEREO;
    default:
        RK_LOGE("channel = %d not support", ch);
        return AUDIO_SOUND_MODE_BUTT;
    }
}

AUDIO_BIT_WIDTH_E AudioCapture::findBitWidth(RK_S32 bit)
{
    switch (bit) {
    case 8:
        return AUDIO_BIT_WIDTH_8;
    case 16:
        return AUDIO_BIT_WIDTH_16;
    case 24:
        return AUDIO_BIT_WIDTH_24;
    default:
        RK_LOGE("bitwidth(%d) not support", bit);
        return AUDIO_BIT_WIDTH_BUTT;
    }
}

RK_S32 AudioCapture::openDevice()
{
    AIO_ATTR_S aiAttr;
    std::memset(&aiAttr, 0, sizeof(AIO_ATTR_S));

    if (gAudioCaptureConfig.cardName) {
        snprintf(reinterpret_cast<char *>(aiAttr.u8CardName),
                 sizeof(aiAttr.u8CardName), "%s", gAudioCaptureConfig.cardName);
    }

    aiAttr.soundCard.channels = gAudioCaptureConfig.deviceChannel;
    aiAttr.soundCard.sampleRate = gAudioCaptureConfig.deviceSampleRate;
    aiAttr.soundCard.bitWidth = AUDIO_BIT_WIDTH_16;

    AUDIO_BIT_WIDTH_E bitWidth = findBitWidth(gAudioCaptureConfig.outBitWidth);
    if (bitWidth == AUDIO_BIT_WIDTH_BUTT) {
        return RK_FAILURE;
    }
    aiAttr.enBitwidth = bitWidth;
    aiAttr.enSamplerate = static_cast<AUDIO_SAMPLE_RATE_E>(gAudioCaptureConfig.outSampleRate);

    AUDIO_SOUND_MODE_E soundMode = findSoundMode(gAudioCaptureConfig.outChannel);
    if (soundMode == AUDIO_SOUND_MODE_BUTT) {
        return RK_FAILURE;
    }
    aiAttr.enSoundmode = soundMode;
    aiAttr.u32FrmNum = gAudioCaptureConfig.periodCount;
    aiAttr.u32PtNumPerFrm = gAudioCaptureConfig.periodSize;

    aiAttr.u32EXFlag = 0;
    aiAttr.u32ChnCnt = 2;

    RK_S32 ret = RK_MPI_AI_SetPubAttr(gAudioCaptureConfig.devId, &aiAttr);
    if (ret != RK_SUCCESS) {
        RK_LOGE("ai set attr fail, reason = %d", ret);
        return RK_FAILURE;
    }

    ret = RK_MPI_AI_Enable(gAudioCaptureConfig.devId);
    if (ret != RK_SUCCESS) {
        RK_LOGE("ai enable fail, reason = %d", ret);
        return RK_FAILURE;
    }

    return RK_SUCCESS;
}

RK_S32 AudioCapture::initChannel()
{
    RK_S32 ret = RK_MPI_AI_EnableChn(gAudioCaptureConfig.devId, gAudioCaptureConfig.chnId);
    if (ret != RK_SUCCESS) {
        RK_LOGE("ai enable channel fail, chn = %d, reason = %x", gAudioCaptureConfig.chnId, ret);
        return RK_FAILURE;
    }

    ret = RK_MPI_AI_EnableReSmp(gAudioCaptureConfig.devId, gAudioCaptureConfig.chnId,
                                static_cast<AUDIO_SAMPLE_RATE_E>(gAudioCaptureConfig.outSampleRate));
    if (ret != RK_SUCCESS) {
        RK_LOGE("ai enable resample fail, reason = %x", ret);
        RK_MPI_AI_DisableChn(gAudioCaptureConfig.devId, gAudioCaptureConfig.chnId);
        return RK_FAILURE;
    }

    return RK_SUCCESS;
}

RK_S32 AudioCapture::deinitChannel()
{
    RK_MPI_AI_DisableReSmp(gAudioCaptureConfig.devId, gAudioCaptureConfig.chnId);
    RK_MPI_AI_DisableChn(gAudioCaptureConfig.devId, gAudioCaptureConfig.chnId);
    RK_MPI_AI_Disable(gAudioCaptureConfig.devId);
    return RK_SUCCESS;
}
