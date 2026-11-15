#ifndef MEDIA_SERVICE_MPP_ENCODER_H
#define MEDIA_SERVICE_MPP_ENCODER_H

#include <cstddef>
#include <cstdint>

struct EncodedPacket {
    uint8_t *data;
    uint32_t size;
    uint64_t pts;
    bool keyframe;
};

int mpp_encoder_init(uint32_t width, uint32_t height, uint32_t hor_stride);
int mpp_encoder_encode(int dmabuf_fd, size_t buf_size, uint64_t pts, EncodedPacket *out);
void mpp_encoder_deinit(void);

#endif
