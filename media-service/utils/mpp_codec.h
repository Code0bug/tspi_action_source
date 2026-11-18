#ifndef MEDIA_SERVICE_MPP_CODEC_H
#define MEDIA_SERVICE_MPP_CODEC_H

#include <cstddef>
#include <cstdint>

/* ------------------------------- 编码器 ---------------------------------  */

struct EncodedPacket {
    uint8_t *data;
    uint32_t size;
    uint64_t pts;
    bool keyframe;
};

int mpp_encoder_init(uint32_t width, uint32_t height, uint32_t hor_stride);
int mpp_encoder_encode(int dmabuf_fd, size_t buf_size, uint64_t pts, EncodedPacket *out);
void mpp_encoder_deinit(void);

/* ------------------------------- 解码器 ---------------------------------  */

struct DecodedFrame {
    int fd;
    uint32_t width;
    uint32_t height;
    uint32_t hor_stride;
    uint32_t ver_stride;
    uint64_t pts;
    bool eos;
    void *handle;
};

enum MppDecResult {
    MPP_DEC_RES_FRAME = 1,
    MPP_DEC_RES_NONE = 0,
    MPP_DEC_RES_ERR = -1,
};

int mpp_decoder_init(void);
int mpp_decoder_put_packet(const uint8_t *data, size_t size, uint64_t pts, bool eos);
int mpp_decoder_get_frame(DecodedFrame *out);
void mpp_decoder_release_frame(DecodedFrame *frame);
void mpp_decoder_deinit(void);

#endif
