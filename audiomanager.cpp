#include "audiomanager.h"

#include <cstring>
#include <climits>
#include <algorithm>
#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

namespace {
constexpr AUDIO_DEV kPlaybackDevId = 0;
constexpr AO_CHN kPlaybackChnId = 0;
constexpr int kPlaybackBitWidth = 16;
constexpr int kPlaybackPeriodCount = 4;
constexpr int kPlaybackPeriodSize = 1024;
constexpr int kPlaybackStopWaitMs = 100;

std::string ffErrStr(int errnum)
{
    char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errnum, errbuf, sizeof(errbuf));
    return std::string(errbuf);
}

uint64_t channelLayoutFromChannels(int channels)
{
    return channels == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;
}
}

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

AudioManager::AudioManager()
    : isRunning(0),
      isPlaying(0),
      aoDevId(kPlaybackDevId),
      aoChnId(kPlaybackChnId),
      playbackSampleRate(0),
      playbackChannels(0),
      playbackPts(0),
      audioDecCtx(nullptr),
      audioSwrCtx(nullptr),
      audioFrame(nullptr)
{
    if (RK_MPI_SYS_Init() != RK_SUCCESS) {
        RK_LOGE("rk mpi sys init fail!");
    }
}

AudioManager::~AudioManager()
{
    if (isRunning) {
        stopCapture();
    }
    stopPlayback();

    deinitChannel();
    RK_MPI_SYS_Exit();
}

void AudioManager::startCapture()
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
    const int bytesPerCodecFrame = 1024 * gAudioCaptureConfig.outChannel * 2; // 1024 samples × channels × 2bytes(s16)
    QByteArray pcmCache;
    uint64_t lastPts = 0;
    while (isRunning) {
        AUDIO_FRAME_S frame;
        RK_S32 ret = RK_MPI_AI_GetFrame(gAudioCaptureConfig.devId, gAudioCaptureConfig.chnId, &frame, RK_NULL, -1);
        if (ret != RK_SUCCESS) {
            continue;
        }

        // printf("Audio PTS = %ld\r\n", frame.u64TimeStamp);

        // 发送给MP4封装音频流
        void *data = RK_MPI_MB_Handle2VirAddr(frame.pMbBlk);
        RK_U64 len = RK_MPI_MB_GetLength(frame.pMbBlk);
        if (data != nullptr && len > 0) {
            int copyLen = (len > static_cast<RK_U64>(INT_MAX)) ? INT_MAX : static_cast<int>(len);
            pcmCache.append(reinterpret_cast<const char *>(data), copyLen);
            lastPts = frame.u64TimeStamp;
            // printf("audio capture pcm bytes: %d, cache: %d\n", copyLen, pcmCache.size());
            
            // AAC 编码帧一次需要1024个采样（1024帧），也就是4096字节
            // RK AI 接收的一帧frame数据大小固定为1024字节，所以要攒4次达到4096字节再发送进行编码
            while (pcmCache.size() >= bytesPerCodecFrame) {
                QByteArray oneFrame = pcmCache.left(bytesPerCodecFrame);
                pcmCache.remove(0, bytesPerCodecFrame);
                emit capFinishedToAudio(oneFrame, lastPts);
            }
        }

        RK_MPI_AI_ReleaseFrame(gAudioCaptureConfig.devId, gAudioCaptureConfig.chnId, &frame, RK_NULL);
    }


    deinitChannel();
}

void AudioManager::stopCapture()
{
    if (!isRunning) {
        return;
    }

    isRunning = 0;
}

void AudioManager::startPlayback(int codecId, int sampleRate, int channels, QByteArray extradata)
{
    stopPlayback();

    if (sampleRate <= 0) {
        sampleRate = gAudioCaptureConfig.outSampleRate > 0 ? gAudioCaptureConfig.outSampleRate : 16000;
    }
    channels = channels == 2 ? 2 : 1;

    const AVCodec *codec = avcodec_find_decoder(static_cast<AVCodecID>(codecId));
    if (!codec) {
        std::cerr << "avcodec_find_decoder failed, codecId=" << codecId << std::endl;
        return;
    }

    audioDecCtx = avcodec_alloc_context3(codec);
    if (!audioDecCtx) {
        return;
    }
    audioDecCtx->sample_rate = sampleRate;
    audioDecCtx->channels = channels;
    audioDecCtx->channel_layout = channelLayoutFromChannels(channels);
    if (!extradata.isEmpty()) {
        audioDecCtx->extradata = reinterpret_cast<uint8_t *>(av_malloc(extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        if (!audioDecCtx->extradata) {
            releasePlaybackDecoder();
            return;
        }
        memcpy(audioDecCtx->extradata, extradata.constData(), static_cast<size_t>(extradata.size()));
        memset(audioDecCtx->extradata + extradata.size(), 0, AV_INPUT_BUFFER_PADDING_SIZE);
        audioDecCtx->extradata_size = extradata.size();
    }

    int ret = avcodec_open2(audioDecCtx, codec, nullptr);
    if (ret < 0) {
        std::cerr << "avcodec_open2(audio decode) failed: " << ffErrStr(ret)
                  << " (ret=" << ret << ")" << std::endl;
        releasePlaybackDecoder();
        return;
    }

    audioFrame = av_frame_alloc();
    if (!audioFrame) {
        releasePlaybackDecoder();
        return;
    }

    playbackSampleRate = sampleRate;
    playbackChannels = channels;
    playbackPts = 0;
    if (openPlaybackDevice(playbackSampleRate, playbackChannels) != RK_SUCCESS ||
        initPlaybackChannel(playbackSampleRate) != RK_SUCCESS) {
        releasePlaybackDecoder();
        deinitPlaybackChannel();
        return;
    }

    isPlaying = 1;
}

void AudioManager::playEncodedAudioPacket(QByteArray packetData, qint64 pts)
{
    if (!isPlaying || !audioDecCtx || packetData.isEmpty()) {
        return;
    }

    decodeAndPlayPacket(reinterpret_cast<const uint8_t *>(packetData.constData()), packetData.size(), pts);
}

void AudioManager::stopPlayback()
{
    const bool wasPlaying = isPlaying;
    isPlaying = 0;

    if (audioDecCtx && wasPlaying) {
        decodeAndPlayPacket(nullptr, 0, 0);
    }

    if (wasPlaying) {
        RK_MPI_AO_WaitEos(aoDevId, aoChnId, kPlaybackStopWaitMs);
        RK_MPI_AO_ClearChnBuf(aoDevId, aoChnId);
    }

    releasePlaybackDecoder();
    deinitPlaybackChannel();
    playbackSampleRate = 0;
    playbackChannels = 0;
    playbackPts = 0;
}

AUDIO_SOUND_MODE_E AudioManager::findSoundMode(RK_S32 ch)
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

AUDIO_BIT_WIDTH_E AudioManager::findBitWidth(RK_S32 bit)
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

RK_S32 AudioManager::openDevice()
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

RK_S32 AudioManager::initChannel()
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

RK_S32 AudioManager::deinitChannel()
{
    RK_MPI_AI_DisableReSmp(gAudioCaptureConfig.devId, gAudioCaptureConfig.chnId);
    RK_MPI_AI_DisableChn(gAudioCaptureConfig.devId, gAudioCaptureConfig.chnId);
    RK_MPI_AI_Disable(gAudioCaptureConfig.devId);
    return RK_SUCCESS;
}

RK_S32 AudioManager::openPlaybackDevice(int sampleRate, int channels)
{
    AIO_ATTR_S aoAttr;
    std::memset(&aoAttr, 0, sizeof(AIO_ATTR_S));

    if (gAudioCaptureConfig.cardName) {
        snprintf(reinterpret_cast<char *>(aoAttr.u8CardName),
                 sizeof(aoAttr.u8CardName), "%s", gAudioCaptureConfig.cardName);
    }

    aoAttr.soundCard.channels = channels;
    aoAttr.soundCard.sampleRate = sampleRate;
    aoAttr.soundCard.bitWidth = AUDIO_BIT_WIDTH_16;

    AUDIO_BIT_WIDTH_E bitWidth = findBitWidth(kPlaybackBitWidth);
    if (bitWidth == AUDIO_BIT_WIDTH_BUTT) {
        return RK_FAILURE;
    }
    aoAttr.enBitwidth = bitWidth;
    aoAttr.enSamplerate = static_cast<AUDIO_SAMPLE_RATE_E>(sampleRate);

    AUDIO_SOUND_MODE_E soundMode = findSoundMode(channels);
    if (soundMode == AUDIO_SOUND_MODE_BUTT) {
        return RK_FAILURE;
    }
    aoAttr.enSoundmode = soundMode;
    aoAttr.u32FrmNum = kPlaybackPeriodCount;
    aoAttr.u32PtNumPerFrm = kPlaybackPeriodSize;
    aoAttr.u32EXFlag = 0;
    aoAttr.u32ChnCnt = 2;

    RK_S32 ret = RK_MPI_AO_SetPubAttr(aoDevId, &aoAttr);
    if (ret != RK_SUCCESS) {
        RK_LOGE("ao set attr fail, reason = %d", ret);
        return RK_FAILURE;
    }

    ret = RK_MPI_AO_Enable(aoDevId);
    if (ret != RK_SUCCESS) {
        RK_LOGE("ao enable fail, reason = %d", ret);
        return RK_FAILURE;
    }

    return RK_SUCCESS;
}

RK_S32 AudioManager::initPlaybackChannel(int sampleRate)
{
    RK_S32 ret = RK_MPI_AO_EnableChn(aoDevId, aoChnId);
    if (ret != RK_SUCCESS) {
        RK_LOGE("ao enable channel fail, chn = %d, reason = %x", aoChnId, ret);
        return RK_FAILURE;
    }

    ret = RK_MPI_AO_EnableReSmp(aoDevId, aoChnId, static_cast<AUDIO_SAMPLE_RATE_E>(sampleRate));
    if (ret != RK_SUCCESS) {
        RK_LOGE("ao enable resample fail, reason = %x", ret);
        RK_MPI_AO_DisableChn(aoDevId, aoChnId);
        return RK_FAILURE;
    }

    return RK_SUCCESS;
}

RK_S32 AudioManager::deinitPlaybackChannel()
{
    RK_MPI_AO_ClearChnBuf(aoDevId, aoChnId);
    RK_MPI_AO_DisableReSmp(aoDevId, aoChnId);
    RK_MPI_AO_DisableChn(aoDevId, aoChnId);
    RK_MPI_AO_Disable(aoDevId);
    return RK_SUCCESS;
}

int AudioManager::decodeAndPlayPacket(const uint8_t *data, int size, qint64 pts)
{
    if (!audioDecCtx || !audioFrame) {
        return -1;
    }

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = const_cast<uint8_t *>(data);
    pkt.size = size;
    pkt.pts = pts;

    int ret = avcodec_send_packet(audioDecCtx, data ? &pkt : nullptr);
    if (ret < 0) {
        std::cerr << "avcodec_send_packet(audio) failed: " << ffErrStr(ret)
                  << " (ret=" << ret << ")" << std::endl;
        return ret;
    }

    while ((ret = avcodec_receive_frame(audioDecCtx, audioFrame)) == 0) {
        int playRet = playDecodedFrame(audioFrame);
        av_frame_unref(audioFrame);
        if (playRet < 0) {
            return playRet;
        }
    }

    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return 0;
    }
    if (ret < 0) {
        std::cerr << "avcodec_receive_frame(audio) failed: " << ffErrStr(ret)
                  << " (ret=" << ret << ")" << std::endl;
    }
    return ret;
}

int AudioManager::playDecodedFrame(AVFrame *frame)
{
    if (!frame) {
        return -1;
    }

    const int outChannels = playbackChannels > 0 ? playbackChannels : std::max(frame->channels, 1);
    const int outSampleRate = playbackSampleRate > 0 ? playbackSampleRate : frame->sample_rate;
    const uint64_t inLayout = frame->channel_layout ? frame->channel_layout : channelLayoutFromChannels(frame->channels);
    const uint64_t outLayout = channelLayoutFromChannels(outChannels);

    if (!audioSwrCtx) {
        audioSwrCtx = swr_alloc_set_opts(
            nullptr,
            outLayout,
            AV_SAMPLE_FMT_S16,
            outSampleRate,
            inLayout,
            static_cast<AVSampleFormat>(frame->format),
            frame->sample_rate,
            0,
            nullptr
        );
        if (!audioSwrCtx) {
            return -1;
        }
        int ret = swr_init(audioSwrCtx);
        if (ret < 0) {
            std::cerr << "swr_init(playback) failed: " << ffErrStr(ret)
                      << " (ret=" << ret << ")" << std::endl;
            return ret;
        }
    }

    const int maxOutSamples = av_rescale_rnd(
        swr_get_delay(audioSwrCtx, frame->sample_rate) + frame->nb_samples,
        outSampleRate,
        frame->sample_rate,
        AV_ROUND_UP
    );
    const int outBytes = av_samples_get_buffer_size(nullptr, outChannels, maxOutSamples, AV_SAMPLE_FMT_S16, 1);
    if (outBytes <= 0) {
        return -1;
    }

    QByteArray pcm(outBytes, 0);
    uint8_t *outData[1] = { reinterpret_cast<uint8_t *>(pcm.data()) };
    int convertedSamples = swr_convert(
        audioSwrCtx,
        outData,
        maxOutSamples,
        const_cast<const uint8_t **>(frame->extended_data),
        frame->nb_samples
    );
    if (convertedSamples < 0) {
        std::cerr << "swr_convert(playback) failed: " << ffErrStr(convertedSamples)
                  << " (ret=" << convertedSamples << ")" << std::endl;
        return convertedSamples;
    }

    const int convertedBytes = av_samples_get_buffer_size(nullptr, outChannels, convertedSamples, AV_SAMPLE_FMT_S16, 1);
    if (convertedBytes <= 0) {
        return 0;
    }
    return sendPcmToAo(reinterpret_cast<const uint8_t *>(pcm.constData()), convertedBytes, playbackPts++);
}

int AudioManager::sendPcmToAo(const uint8_t *data, int size, uint64_t pts)
{
    if (!data || size <= 0) {
        return 0;
    }

    RK_U8 *pcmData = reinterpret_cast<RK_U8 *>(calloc(static_cast<size_t>(size), sizeof(RK_U8)));
    if (!pcmData) {
        return -1;
    }
    memcpy(pcmData, data, static_cast<size_t>(size));

    AUDIO_FRAME_S frame;
    memset(&frame, 0, sizeof(frame));
    frame.u32Len = size;
    frame.u64TimeStamp = pts;
    frame.enBitWidth = findBitWidth(kPlaybackBitWidth);
    frame.enSoundMode = findSoundMode(playbackChannels);
    frame.bBypassMbBlk = RK_FALSE;

    MB_EXT_CONFIG_S extConfig;
    memset(&extConfig, 0, sizeof(extConfig));
    extConfig.pFreeCB = nullptr;
    extConfig.pOpaque = pcmData;
    extConfig.pu8VirAddr = pcmData;
    extConfig.u64Size = size;

    RK_S32 ret = RK_MPI_SYS_CreateMB(&(frame.pMbBlk), &extConfig);
    if (ret != RK_SUCCESS) {
        free(pcmData);
        RK_LOGE("create ao mb failed, reason = %x", ret);
        return ret;
    }

    while (isPlaying) {
        ret = RK_MPI_AO_SendFrame(aoDevId, aoChnId, &frame, -1);
        if (ret >= 0) {
            break;
        }
        RK_LOGE("ao send frame fail, reason = %x", ret);
    }

    RK_MPI_MB_ReleaseMB(frame.pMbBlk);
    free(pcmData);
    return ret;
}

void AudioManager::releasePlaybackDecoder()
{
    if (audioFrame) {
        av_frame_free(&audioFrame);
    }
    if (audioSwrCtx) {
        swr_free(&audioSwrCtx);
    }
    if (audioDecCtx) {
        avcodec_free_context(&audioDecCtx);
    }
}
