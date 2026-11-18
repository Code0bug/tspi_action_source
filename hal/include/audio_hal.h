#ifndef __AUDIO_HAL_H__
#define __AUDIO_HAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <alsa/asoundlib.h>

typedef struct {
    const char *device;         // PCM 设备名
    unsigned int sample_rate;   // 采样率
    unsigned int channels;      // 通道数
    unsigned int bits;          // 样本位宽
    unsigned int period_frames; // 每次读取/写入帧数
    int playback;               // 0 采集，1 播放
} audio_config_t;

typedef struct {
    uint8_t *data;
    uint32_t length;
    uint64_t timestamp_us;
} audio_frame_t;

typedef struct {
    snd_pcm_t *pcm;
    int playback;
    unsigned int sample_rate;
    unsigned int channels;
    unsigned int bits;
    snd_pcm_format_t format;
    uint32_t bytes_per_frame;
    snd_pcm_uframes_t period_frames;
    uint32_t bytes_per_period;
    uint8_t *buffer;
} audio_t;

audio_t *audio_init(const audio_config_t *config);

int audio_get_frame(audio_t *ctx, audio_frame_t *frame);

int audio_release_frame(audio_t *ctx, audio_frame_t *frame);

int audio_write(audio_t *ctx, const uint8_t *data, uint32_t length);

int audio_flush(audio_t *ctx);

int audio_drain(audio_t *ctx);

void audio_deinit(audio_t *ctx);

#ifdef __cplusplus
}
#endif

#endif
