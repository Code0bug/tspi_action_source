#include "audio_manager.h"

#include "media_manager.h"

#include <audio_hal.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

namespace {
constexpr int kPlaybackBitWidth = 16;
constexpr int kPlaybackPeriodSize = 1024;

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

static const audio_config_t kAudioConfig = {
    "default", // device
    16000,     // sample_rate
    2,         // channels
    16,        // bits
    1024,      // period_frames
};

AudioManager::AudioManager(MediaManager &media)
    : media_(media)
{
}

AudioManager::~AudioManager()
{
}

void AudioManager::start()
{
    audio_capture = audio_init(&kAudioConfig);
    if (audio_capture == nullptr)
        std::fprintf(stderr, "AudioManager: audio_init failed\n");

    audio_thread = std::thread(&AudioManager::captureRun, this);
}

void AudioManager::stop()
{
    running_ = false;
    capturing_ = false;
    cv_.notify_one();
    audio_thread.join();
    audio_deinit(audio_capture);

    // 播放线程可能阻塞在队列 pop 上，推送结束标志使其退出
    DemuxJob job = {};
    job.type = DemuxJobType::AudioEnd;
    media_.demux_queue.push(std::move(job));
}

void AudioManager::startCapture()
{
    capturing_ = true;
    cv_.notify_one();
}

void AudioManager::stopCapture()
{
    capturing_ = false;
}

void AudioManager::startPlayback()
{
    // detach：线程取到 AudioEnd 退出后自动回收，下次播放可重新创建
    std::thread playback_thread(&AudioManager::playbackRun, this);
    playback_thread.detach();
}

void AudioManager::playbackRun()
{
    for (;;) {
        DemuxJob job = media_.demux_queue.pop();
        switch (job.type) {
        case DemuxJobType::AudioStart:
            openPlayback(job.codec_id, job.sample_rate, job.channels, job.extradata);
            break;
        case DemuxJobType::AudioPacket:
            playEncodedAudioPacket(job.data, job.pts);
            break;
        case DemuxJobType::AudioEnd:
            closePlayback();
            return;
        }
    }
}

bool AudioManager::openPlayback(int codecId, int sampleRate, int channels,
                                const std::vector<uint8_t> &extradata)
{
    closePlayback();

    if (sampleRate <= 0) {
        sampleRate = kAudioConfig.sample_rate > 0 ? kAudioConfig.sample_rate : 16000;
    }
    channels = channels == 2 ? 2 : 1;

    const AVCodec *codec = avcodec_find_decoder(static_cast<AVCodecID>(codecId));
    if (!codec) {
        std::cerr << "avcodec_find_decoder failed, codecId=" << codecId << std::endl;
        return false;
    }

    audioDecCtx = avcodec_alloc_context3(codec);
    if (!audioDecCtx) {
        return false;
    }

    audioDecCtx->sample_rate = sampleRate;
    audioDecCtx->channels = channels;
    audioDecCtx->channel_layout = channelLayoutFromChannels(channels);
    if (!extradata.empty()) {
        audioDecCtx->extradata = reinterpret_cast<uint8_t *>(av_malloc(extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        if (!audioDecCtx->extradata) {
            releasePlaybackDecoder();
            return false;
        }
        memcpy(audioDecCtx->extradata, extradata.data(), extradata.size());
        memset(audioDecCtx->extradata + extradata.size(), 0, AV_INPUT_BUFFER_PADDING_SIZE);
        audioDecCtx->extradata_size = static_cast<int>(extradata.size());
    }

    int ret = avcodec_open2(audioDecCtx, codec, nullptr);
    if (ret < 0) {
        std::cerr << "avcodec_open2(audio decode) failed: " << ffErrStr(ret)
                  << " (ret=" << ret << ")" << std::endl;
        releasePlaybackDecoder();
        return false;
    }

    audioFrame = av_frame_alloc();
    if (!audioFrame) {
        releasePlaybackDecoder();
        return false;
    }

    // 打开播放设备（ALSA 播放）
    audio_config_t playbackConfig = {};
    playbackConfig.device = kAudioConfig.device;
    playbackConfig.sample_rate = static_cast<unsigned int>(sampleRate);
    playbackConfig.channels = static_cast<unsigned int>(channels);
    playbackConfig.bits = kPlaybackBitWidth;
    playbackConfig.period_frames = kPlaybackPeriodSize;
    playbackConfig.playback = 1;
    audio_playback = audio_init(&playbackConfig);
    if (audio_playback == nullptr) {
        std::fprintf(stderr, "AudioManager: audio_init(playback) failed\n");
        releasePlaybackDecoder();
        return false;
    }

    // 设备实际协商出的采样率可能与请求值不同，重采样输出以设备为准
    playbackSampleRate = static_cast<int>(audio_playback->sample_rate);
    playbackChannels = channels;
    isPlaying = true;
    return true;
}

void AudioManager::playEncodedAudioPacket(const std::vector<uint8_t> &packetData, int64_t pts)
{
    if (!isPlaying || !audioDecCtx || packetData.empty()) {
        return;
    }

    decodeAndPlayPacket(packetData.data(), static_cast<int>(packetData.size()), pts);
}

void AudioManager::closePlayback()
{
    const bool wasPlaying = isPlaying;

    if (audioDecCtx && wasPlaying) {
        decodeAndPlayPacket(nullptr, 0, 0);
    }

    isPlaying = false;

    if (audio_playback) {
        if (wasPlaying) {
            audio_drain(audio_playback);
        }
        audio_deinit(audio_playback);
        audio_playback = nullptr;
    }

    releasePlaybackDecoder();
    playbackSampleRate = 0;
    playbackChannels = 0;
}

int AudioManager::decodeAndPlayPacket(const uint8_t *data, int size, int64_t pts)
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

    std::vector<uint8_t> pcm(static_cast<size_t>(outBytes), 0);
    uint8_t *outData[1] = { pcm.data() };
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
    return sendPcmToAudio(pcm.data(), convertedBytes);
}

int AudioManager::sendPcmToAudio(const uint8_t *data, int size)
{
    if (!data || size <= 0 || !audio_playback || !isPlaying) {
        return 0;
    }

    return audio_write(audio_playback, data, static_cast<uint32_t>(size));
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

void AudioManager::captureRun()
{
    const int bytesPerCodecFrame = 1024 * 2 * 2; // 1024 samples × 2ch × 2bytes(s16)

    std::vector<uint8_t> pcmCache;
    uint64_t lastPts = 0;

    for (;;) {
        if (!capturing_.load())
            pcmCache.clear(); // stopCapture 后丢弃不足一帧的残留数据

        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this] { return !running_.load() || capturing_.load(); });
            if (!running_)
                return;
        }

        audio_frame_t frame;
        if (audio_get_frame(audio_capture, &frame) != 0)
            continue;

        pcmCache.insert(pcmCache.end(), frame.data, frame.data + frame.length);
        lastPts = frame.timestamp_us;
        // AAC 编码要求编码帧必须是 1024 个采样点
        while ((int)pcmCache.size() >= bytesPerCodecFrame) {
            MuxJob job = {};
            job.type = MuxJobType::Audio;
            job.pcm.assign(pcmCache.begin(),
                           pcmCache.begin() + bytesPerCodecFrame);
            job.pts = lastPts;
            media_.mux_queue.push(std::move(job));
            pcmCache.erase(pcmCache.begin(),
                           pcmCache.begin() + bytesPerCodecFrame);
        }
    }
}
