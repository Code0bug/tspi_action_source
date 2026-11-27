#ifndef MEDIA_SERVICE_AUDIO_MANAGER_H
#define MEDIA_SERVICE_AUDIO_MANAGER_H

#include <audio_hal.h>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

struct AVCodecContext;
struct AVFrame;
struct SwrContext;

class MediaManager;

class AudioManager {
public:
    explicit AudioManager(MediaManager &media);
    ~AudioManager();

    AudioManager(const AudioManager &) = delete;
    AudioManager &operator=(const AudioManager &) = delete;

    void stop();
    void captureRun();
    void startCapture();
    void stopCapture();
    void startPlayback();
    void playbackRun();

private:
    bool openPlayback(int codecId, int sampleRate, int channels, const std::vector<uint8_t> &extradata);
    void playEncodedAudioPacket(const std::vector<uint8_t> &packetData, int64_t pts);
    void closePlayback();
    int decodeAndPlayPacket(const uint8_t *data, int size, int64_t pts);
    int playDecodedFrame(AVFrame *frame);
    void releasePlaybackDecoder();

    MediaManager &media_;
    audio_t *audio_capture = nullptr;
    audio_t *audio_playback = nullptr;
    bool playbackReady = false;
    int playbackTrueSampleRate = 0;
    int playbackTrueChannels = 0;
    AVCodecContext *audioDecCtx = nullptr;
    SwrContext *audioSwrCtx = nullptr;
    AVFrame *audioFrame = nullptr;
    std::atomic<bool> capturing_{false};
    std::thread audio_thread;
};

#endif
