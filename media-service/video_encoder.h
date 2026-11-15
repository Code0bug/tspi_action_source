#ifndef MEDIA_SERVICE_VIDEO_ENCODER_H
#define MEDIA_SERVICE_VIDEO_ENCODER_H

#include <cstddef>
#include <cstdint>

#include "rk_mpi.h"
#include "rk_venc_cfg.h"
#include "rk_venc_rc.h"
#include "mpp_buffer.h"
#include "mpp_frame.h"
#include "mpp_packet.h"

#define encWidth 1920
#define encHeight 1080

struct EncodedPacket {
    uint8_t *data;
    uint32_t size;
    uint64_t pts;
    bool keyframe;
};

class VideoEncoder {
public:
    VideoEncoder();
    ~VideoEncoder();

    VideoEncoder(const VideoEncoder &) = delete;
    VideoEncoder &operator=(const VideoEncoder &) = delete;

    int init(uint32_t width, uint32_t height, uint32_t hor_stride);
    int encode(int dmabuf_fd, size_t buf_size, uint64_t pts, EncodedPacket *out);

private:
    void deinit();
    void releasePacket();

    MppCtx ctx;
    MppApi *mpi;
    MppEncCfg cfg;
    MppBufferGroup buf_grp;
    MppBuffer pkt_buf;
    MppPacket packet;
    RK_U32 width_;
    RK_U32 height_;
    RK_U32 hor_stride_;
    MppCodingType type;
    MppFrameFormat fmt;
};

#endif
