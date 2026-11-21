#define MODULE_TAG "mpp_codec"

#include "mpp_codec.h"

#include "rk_mpi.h"
#include "rk_venc_cfg.h"
#include "rk_venc_rc.h"
#include "mpp_buffer.h"
#include "mpp_frame.h"
#include "mpp_packet.h"
#include "mpp_meta.h"

#include <cstdio>
#include <cstring>

/* ------------------------------- 编码器 ---------------------------------  */

static MppCtx ctx;
static MppApi *mpi;
static MppEncCfg cfg;
static MppBufferGroup buf_grp;
static MppBuffer pkt_buf;
static MppPacket packet;
static RK_U32 width_;
static RK_U32 height_;
static RK_U32 hor_stride_;
static MppCodingType type;
static MppFrameFormat fmt;

static void release_packet(void)
{
    if (packet) {
        mpp_packet_deinit(&packet);
        packet = nullptr;
    }
}

int mpp_encoder_init(uint32_t width, uint32_t height, uint32_t hor_stride)
{
    int ret;
    width_ = width;
    height_ = height;
    hor_stride_ = hor_stride;
    type = MPP_VIDEO_CodingAVC;     // H.264
    fmt  = MPP_FMT_YUV420SP;        // NV12 格式

    ret = mpp_create(&ctx, &mpi);
    if (ret) {
        printf("mpp_create failed %d\n", ret);
        return ret;
    }

    ret = mpp_init(ctx, MPP_CTX_ENC, type);
    if (ret) {
        printf("mpp_init failed %d\n", ret);
        return ret;
    }

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
    mpp_enc_cfg_set_s32(cfg, "rc:gop", 30);  // GOP 序列长度
    mpp_enc_cfg_set_s32(cfg, "rc:fps_out_num", 30);
    mpp_enc_cfg_set_s32(cfg, "codec:type", type);
    mpi->control(ctx, MPP_ENC_SET_CFG, cfg);

    // 让每个 IDR 前都输出 SPS/PPS 信息
    // 也可以通过每次录像都重新初始化 encoder，来避免找不到 SPS/PPS 的问题
    // MppEncHeaderMode header_mode = MPP_ENC_HEADER_MODE_EACH_IDR;
    // ret = mpi->control(ctx, MPP_ENC_SET_HEADER_MODE, &header_mode);
    // if (ret)
    //     printf("mpp_encoder: set header mode failed %d\n", ret);

    RK_S64 timeout = 1000;
    mpi->control(ctx, MPP_SET_INPUT_TIMEOUT, &timeout);
    mpi->control(ctx, MPP_SET_OUTPUT_TIMEOUT, &timeout);

    mpp_buffer_group_get_internal(&buf_grp, MPP_BUFFER_TYPE_DRM);
    size_t frame_size = hor_stride * height * 3 / 2;
    mpp_buffer_get(buf_grp, &pkt_buf, frame_size);

    return 0;
}

int mpp_encoder_encode(int dmabuf_fd, size_t buf_size, uint64_t pts, EncodedPacket *out)
{
    int ret = 0;
    MppFrame frame = nullptr;
    MppBuffer frm_buf = nullptr;
    MppBufferInfo info;

    release_packet();

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

void mpp_encoder_deinit(void)
{
    release_packet();
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

/* ------------------------------- 解码器 ---------------------------------  */

static MppCtx dec_ctx;
static MppApi *dec_mpi;

int mpp_decoder_init(void)
{
    int ret = mpp_create(&dec_ctx, &dec_mpi);
    if (ret) {
        printf("mpp_decoder: mpp_create failed %d\n", ret);
        return ret;
    }

    RK_U32 need_split = 0;
    dec_mpi->control(dec_ctx, MPP_DEC_SET_PARSER_SPLIT_MODE, &need_split);
    RK_U32 fast_mode = 1;
    dec_mpi->control(dec_ctx, MPP_DEC_SET_PARSER_FAST_MODE, &fast_mode);

    ret = mpp_init(dec_ctx, MPP_CTX_DEC, MPP_VIDEO_CodingAVC);
    if (ret) {
        printf("mpp_decoder: mpp_init failed %d\n", ret);
        mpp_destroy(dec_ctx);
        dec_ctx = nullptr;
        dec_mpi = nullptr;
        return ret;
    }

    RK_S64 in_timeout = 20;
    RK_S64 out_timeout = 100;
    dec_mpi->control(dec_ctx, MPP_SET_INPUT_TIMEOUT, &in_timeout);
    dec_mpi->control(dec_ctx, MPP_SET_OUTPUT_TIMEOUT, &out_timeout);

    MppFrameFormat out_fmt = MPP_FMT_YUV420SP;
    dec_mpi->control(dec_ctx, MPP_DEC_SET_OUTPUT_FORMAT, &out_fmt);
    return 0;
}

int mpp_decoder_put_packet(const uint8_t *data, size_t size, uint64_t pts, bool eos)
{
    if (!dec_ctx)
        return -1;

    MppPacket pkt = nullptr;
    int ret = mpp_packet_init(&pkt, const_cast<uint8_t *>(data), size);
    if (ret) {
        printf("mpp_decoder: mpp_packet_init failed %d\n", ret);
        return -1;
    }
    mpp_packet_set_pts(pkt, (RK_S64)pts);
    if (eos)
        mpp_packet_set_eos(pkt);

    ret = dec_mpi->decode_put_packet(dec_ctx, pkt);
    mpp_packet_deinit(&pkt);
    if (ret == MPP_ERR_BUFFER_FULL)
        return 1;
    if (ret) {
        printf("mpp_decoder: decode_put_packet failed %d\n", ret);
        return -1;
    }
    return 0;
}

int mpp_decoder_get_frame(DecodedFrame *out)
{
    if (!dec_ctx || !out)
        return MPP_DEC_RES_ERR;

    memset(out, 0, sizeof(*out));
    out->fd = -1;

    MppFrame frame = nullptr;
    int ret = dec_mpi->decode_get_frame(dec_ctx, &frame);
    if (ret == MPP_ERR_TIMEOUT)
        return MPP_DEC_RES_NONE;
    if (ret) {
        printf("mpp_decoder: decode_get_frame failed %d\n", ret);
        return MPP_DEC_RES_ERR;
    }
    if (!frame)
        return MPP_DEC_RES_NONE;

    if (mpp_frame_get_info_change(frame)) {
        printf("mpp_decoder: info change %ux%u stride %ux%u\n",
               mpp_frame_get_width(frame), mpp_frame_get_height(frame),
               mpp_frame_get_hor_stride(frame), mpp_frame_get_ver_stride(frame));
        dec_mpi->control(dec_ctx, MPP_DEC_SET_INFO_CHANGE_READY, nullptr);
        mpp_frame_deinit(&frame);
        return MPP_DEC_RES_NONE;
    }

    if (mpp_frame_get_eos(frame)) {
        out->eos = true;
        mpp_frame_deinit(&frame);
        return MPP_DEC_RES_FRAME;
    }

    MppBuffer buf = mpp_frame_get_buffer(frame);
    if (mpp_frame_get_errinfo(frame) || mpp_frame_get_discard(frame) || !buf) {
        mpp_frame_deinit(&frame);
        return MPP_DEC_RES_NONE;
    }

    out->fd = mpp_buffer_get_fd(buf);
    out->width = mpp_frame_get_width(frame);
    out->height = mpp_frame_get_height(frame);
    out->hor_stride = mpp_frame_get_hor_stride(frame);
    out->ver_stride = mpp_frame_get_ver_stride(frame);
    out->pts = static_cast<uint64_t>(mpp_frame_get_pts(frame));
    out->handle = frame;
    return MPP_DEC_RES_FRAME;
}

void mpp_decoder_release_frame(DecodedFrame *frame)
{
    if (!frame || !frame->handle)
        return;
    MppFrame f = static_cast<MppFrame>(frame->handle);
    mpp_frame_deinit(&f);
    frame->handle = nullptr;
    frame->fd = -1;
}

void mpp_decoder_deinit(void)
{
    if (dec_ctx) {
        mpp_destroy(dec_ctx);
        dec_ctx = nullptr;
        dec_mpi = nullptr;
    }
}
