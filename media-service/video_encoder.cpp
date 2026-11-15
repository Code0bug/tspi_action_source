#define MODULE_TAG "video_encoder"

#include "video_encoder.h"

#include "mpp_meta.h"

#include <cstdio>
#include <cstring>

VideoEncoder::VideoEncoder()
    : ctx(nullptr)
    , mpi(nullptr)
    , cfg(nullptr)
    , buf_grp(nullptr)
    , pkt_buf(nullptr)
    , packet(nullptr)
    , width_(0)
    , height_(0)
    , hor_stride_(0)
    , type(MPP_VIDEO_CodingAVC)
    , fmt(MPP_FMT_YUV420SP)
{
}

VideoEncoder::~VideoEncoder()
{
    deinit();
}

int VideoEncoder::init(uint32_t width, uint32_t height, uint32_t hor_stride)
{
    int ret;
    width_ = width;
    height_ = height;
    hor_stride_ = hor_stride;
    type = MPP_VIDEO_CodingAVC;     // H.264
    fmt  = MPP_FMT_YUV420SP;        // NV12 格式

    // 2. 创建编码器上下文
    ret = mpp_create(&ctx, &mpi);
    if (ret) {
        printf("mpp_create failed %d\n", ret);
        return ret;
    }

    // 3. 初始化编码器
    ret = mpp_init(ctx, MPP_CTX_ENC, type);
    if (ret) {
        printf("mpp_init failed %d\n", ret);
        return ret;
    }

    // 4. 获取编码配置
    ret = mpp_enc_cfg_init(&cfg);
    if (ret) {
        printf("mpp_enc_cfg_init failed %d\n", ret);
        return ret;
    }

    mpi->control(ctx, MPP_ENC_GET_CFG, cfg);
    mpp_enc_cfg_set_s32(cfg, "prep:width", width);
    mpp_enc_cfg_set_s32(cfg, "prep:height", height);
    mpp_enc_cfg_set_s32(cfg, "prep:hor_stride", hor_stride);
    mpp_enc_cfg_set_s32(cfg, "prep:ver_stride", height);
    mpp_enc_cfg_set_s32(cfg, "prep:format", fmt);
    mpp_enc_cfg_set_s32(cfg, "rc:mode", MPP_ENC_RC_MODE_CBR);
    mpp_enc_cfg_set_s32(cfg, "rc:bps_target", width * height * 8);
    mpp_enc_cfg_set_s32(cfg, "rc:fps_in_num", 30);
    mpp_enc_cfg_set_s32(cfg, "rc:fps_out_num", 30);
    mpp_enc_cfg_set_s32(cfg, "codec:type", type);
    mpi->control(ctx, MPP_ENC_SET_CFG, cfg);

    RK_S64 timeout = 1000;
    mpi->control(ctx, MPP_SET_INPUT_TIMEOUT, &timeout);
    mpi->control(ctx, MPP_SET_OUTPUT_TIMEOUT, &timeout);

    // 5. 分配缓冲区
    mpp_buffer_group_get_internal(&buf_grp, MPP_BUFFER_TYPE_DRM);
    size_t frame_size = hor_stride * height * 3 / 2;
    mpp_buffer_get(buf_grp, &pkt_buf, frame_size);

    return 0;
}

int VideoEncoder::encode(int dmabuf_fd, size_t buf_size, uint64_t pts, EncodedPacket *out)
{
    int ret = 0;
    MppFrame frame = nullptr;
    MppBuffer frm_buf = nullptr;
    MppBufferInfo info;

    releasePacket();

    memset(&info, 0, sizeof(info));
    info.type = MPP_BUFFER_TYPE_EXT_DMA;
    info.fd = dmabuf_fd;
    info.size = buf_size & 0x07ffffff;
    info.index = (buf_size & 0xf8000000) >> 27;
    mpp_buffer_import(&frm_buf, &info);

    mpp_frame_init(&frame);
    mpp_frame_set_width(frame, width_);
    mpp_frame_set_height(frame, height_);
    mpp_frame_set_hor_stride(frame, hor_stride_);
    mpp_frame_set_ver_stride(frame, height_);
    mpp_frame_set_fmt(frame, fmt);
    mpp_frame_set_buffer(frame, frm_buf);
    mpp_frame_set_pts(frame, (RK_S64)pts);

    mpp_packet_init_with_buffer(&packet, pkt_buf);
    mpp_packet_set_length(packet, 0);

    ret = mpi->encode_put_frame(ctx, frame);
    if (ret) {
        printf("encode_put_frame failed %d\n", ret);
        mpp_packet_deinit(&packet);
        packet = nullptr;
        mpp_frame_deinit(&frame);
        mpp_buffer_put(frm_buf);
        return -1;
    }

    ret = mpi->encode_get_packet(ctx, &packet);
    if (ret) {
        printf("encode_get_packet failed %d\n", ret);
        mpp_packet_deinit(&packet);
        packet = nullptr;
        mpp_frame_deinit(&frame);
        mpp_buffer_put(frm_buf);
        return -1;
    }

    void *ptr = mpp_packet_get_pos(packet);
    size_t len = mpp_packet_get_length(packet);
    RK_S32 is_intra = 0;
    if (mpp_packet_has_meta(packet)) {
        MppMeta meta = mpp_packet_get_meta(packet);
        mpp_meta_get_s32(meta, KEY_OUTPUT_INTRA, &is_intra);
    }

    out->data = reinterpret_cast<uint8_t *>(ptr);
    out->size = static_cast<uint32_t>(len);
    out->pts = static_cast<uint64_t>(mpp_packet_get_pts(packet));
    out->keyframe = (is_intra != 0);

    mpp_frame_deinit(&frame);
    mpp_buffer_put(frm_buf);
    return 0;
}

void VideoEncoder::releasePacket()
{
    if (packet) {
        mpp_packet_deinit(&packet);
        packet = nullptr;
    }
}

void VideoEncoder::deinit()
{
    releasePacket();
    if (pkt_buf) {
        mpp_buffer_put(pkt_buf);
        pkt_buf = nullptr;
    }
    if (buf_grp) {
        mpp_buffer_group_put(buf_grp);
        buf_grp = nullptr;
    }
    if (cfg) {
        mpp_enc_cfg_deinit(cfg);
        cfg = nullptr;
    }
    if (ctx) {
        mpp_destroy(ctx);
        ctx = nullptr;
        mpi = nullptr;
    }
}
