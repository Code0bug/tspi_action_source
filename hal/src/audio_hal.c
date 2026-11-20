#define _GNU_SOURCE
#include "audio_hal.h"

#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>

static snd_pcm_format_t bits_to_pcm_format(unsigned int bits)
{
    switch (bits) {
    case 8:
        return SND_PCM_FORMAT_U8;
    case 16:
        return SND_PCM_FORMAT_S16_LE;
    case 24:
        return SND_PCM_FORMAT_S24_LE;
    case 32:
        return SND_PCM_FORMAT_S32_LE;
    default:
        return SND_PCM_FORMAT_UNKNOWN;
    }
}

audio_t *audio_init(const audio_config_t *config)
{
    snd_pcm_format_t format = bits_to_pcm_format(config->bits);
    if (format == SND_PCM_FORMAT_UNKNOWN) {
        fprintf(stderr, "audio_init: unsupported bits %u (supported: 8/16/24/32)\n",
                config->bits);
        return NULL;
    }

    audio_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
        return NULL;
    ctx->sample_rate = config->sample_rate;
    ctx->channels = config->channels;
    ctx->bits = config->bits;
    ctx->format = format;
    ctx->period_frames = config->period_frames;
    ctx->bytes_per_frame = (config->bits / 8) * config->channels;
    ctx->playback = config->playback;

    int rc = snd_pcm_open(&ctx->pcm, config->device,
                          ctx->playback ? SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) {
        fprintf(stderr, "audio_init: snd_pcm_open(%s) failed: %s\n", config->device,
                snd_strerror(rc));
        goto fail;
    }

    int dir = 0;
    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(ctx->pcm, params);
    snd_pcm_hw_params_set_access(ctx->pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(ctx->pcm, params, format);
    snd_pcm_hw_params_set_channels(ctx->pcm, params, ctx->channels);
    snd_pcm_hw_params_set_rate_near(ctx->pcm, params, &ctx->sample_rate, &dir);
    snd_pcm_hw_params_set_period_size_near(ctx->pcm, params, &ctx->period_frames, &dir);
    if (ctx->playback) {
        snd_pcm_uframes_t buffer_frames = ctx->period_frames * 4;
        snd_pcm_hw_params_set_buffer_size_near(ctx->pcm, params, &buffer_frames); // 播放缓冲大小
    }
    rc = snd_pcm_hw_params(ctx->pcm, params);
    if (rc < 0) {
        fprintf(stderr, "audio_init: snd_pcm_hw_params failed: %s\n", snd_strerror(rc));
        goto fail;
    }

    snd_pcm_sw_params_t *swparams;
    snd_pcm_sw_params_alloca(&swparams);
    snd_pcm_sw_params_current(ctx->pcm, swparams);
    if (ctx->playback) {
        snd_pcm_sw_params_set_start_threshold(ctx->pcm, swparams, ctx->period_frames); // 启动阈值
    } else {
        snd_pcm_sw_params_set_tstamp_mode(ctx->pcm, swparams, SND_PCM_TSTAMP_ENABLE);
        snd_pcm_sw_params_set_tstamp_type(ctx->pcm, swparams, SND_PCM_TSTAMP_TYPE_MONOTONIC);
    }
    rc = snd_pcm_sw_params(ctx->pcm, swparams);
    if (rc < 0) {
        fprintf(stderr, "audio_init: snd_pcm_sw_params failed: %s\n", snd_strerror(rc));
        goto fail;
    }

    if (ctx->playback)
        return ctx;

    // 每次读取的字节数
    ctx->bytes_per_period = (uint32_t)ctx->period_frames * ctx->bytes_per_frame;
    // 分配读取缓冲
    ctx->buffer = malloc(ctx->bytes_per_period);
    if (!ctx->buffer) {
        perror("audio_init: malloc");
        goto fail;
    }

    return ctx;

fail:
    if (ctx->pcm)
        snd_pcm_close(ctx->pcm);
    free(ctx->buffer);
    free(ctx);
    return NULL;
}

int audio_get_frame(audio_t *ctx, audio_frame_t *frame)
{
    if (ctx->playback)
        return -EINVAL;

    snd_pcm_sframes_t got = snd_pcm_readi(ctx->pcm, ctx->buffer, ctx->period_frames);
    if (got == -EPIPE) {
        snd_pcm_prepare(ctx->pcm);
        return -EPIPE;
    }
    if (got < 0) {
        fprintf(stderr, "audio_get_frame: snd_pcm_readi failed: %s\n",
                snd_strerror((int)got));
        return (int)got;
    }

    snd_pcm_status_t *status;
    snd_pcm_status_alloca(&status);
    int rc = snd_pcm_status(ctx->pcm, status);
    if (rc < 0) {
        fprintf(stderr, "audio_get_frame: snd_pcm_status failed: %s\n", snd_strerror(rc));
        return rc;
    }
    snd_htimestamp_t ts;
    snd_pcm_status_get_htstamp(status, &ts);

    frame->data = ctx->buffer;
    frame->length = (uint32_t)got * ctx->bytes_per_frame;
    frame->timestamp_us = (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
    return 0;
}

int audio_write(audio_t *ctx, const uint8_t *data, uint32_t length)
{
    if (!ctx->playback)
        return -EINVAL;

    snd_pcm_uframes_t remain = length / ctx->bytes_per_frame;

    while (remain > 0) {
        snd_pcm_sframes_t written = snd_pcm_writei(ctx->pcm, data, remain);
        if (written < 0) {
            written = snd_pcm_recover(ctx->pcm, (int)written, 1);
            if (written < 0) {
                fprintf(stderr, "audio_write: snd_pcm_writei failed: %s\n",
                        snd_strerror((int)written));
                return (int)written;
            }
            continue;
        }
        data += (uint32_t)written * ctx->bytes_per_frame;
        remain -= (snd_pcm_uframes_t)written;
    }
    return 0;
}

int audio_flush(audio_t *ctx)
{
    if (!ctx->playback)
        return -EINVAL;

    int rc = snd_pcm_drop(ctx->pcm);
    if (rc < 0)
        return rc;
    return snd_pcm_prepare(ctx->pcm);
}

int audio_drain(audio_t *ctx)
{
    if (!ctx->playback)
        return -EINVAL;

    return snd_pcm_drain(ctx->pcm);
}

void audio_deinit(audio_t *ctx)
{
    snd_pcm_close(ctx->pcm);
    free(ctx->buffer);
    free(ctx);
}
