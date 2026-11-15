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
    stop();
}

void AudioManager::start()
{
}

void AudioManager::stop()
{
    running_ = false;
    capturing_ = false;
    cv_.notify_one();
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

void AudioManager::run()
{
    const int bytesPerCodecFrame = 1024 * 2 * 2; // 1024 samples × 2ch × 2bytes(s16)

    for (;;) {
        {
            std::unique_lock<std::mutex> lock(mu_);
            cv_.wait(lock, [this] { return !running_.load() || capturing_.load(); });
            if (!running_)
                return;
        }

        audio_t *ctx = audio_init(&kAudioConfig);
        if (ctx == nullptr) {
            std::fprintf(stderr, "AudioManager: audio_init failed\n");
            capturing_ = false;
            continue;
        }

        std::vector<uint8_t> pcmCache;
        uint64_t lastPts = 0;
        while (capturing_.load() && running_.load()) {
            audio_frame_t frame;
            if (audio_get_frame(ctx, &frame) != 0)
                continue;

            pcmCache.insert(pcmCache.end(), frame.data, frame.data + frame.length);
            lastPts = frame.timestamp_us;
            audio_release_frame(ctx, &frame);

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
        audio_deinit(ctx);
    }
}
