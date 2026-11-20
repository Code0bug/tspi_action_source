#include "audio_manager.h"

#include "media_manager.h"

#include <audio_hal.h>

#include <cstdio>
#include <vector>

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
