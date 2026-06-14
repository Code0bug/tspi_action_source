#include "videoencoder.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/error.h>
}
#include <iostream>
#include <string>
#include <cstring>


VideoEncoder::VideoEncoder()
{
    stFrame.pstPack = reinterpret_cast<VENC_PACK_S *>(malloc(sizeof(VENC_PACK_S)));

    // 配置编码帧信息（pMbBlk 每帧动态设置）
    h264_frame.stVFrame.u32Width = encWidth;
    h264_frame.stVFrame.u32Height = encHeight;
    h264_frame.stVFrame.u32VirWidth = encWidth;
    h264_frame.stVFrame.u32VirHeight = encHeight;
    h264_frame.stVFrame.enPixelFormat = RK_FMT_YUV420SP;
    h264_frame.stVFrame.u32FrameFlag = 160;

    // rkmpi init
    if (RK_MPI_SYS_Init() != RK_SUCCESS) {
        RK_LOGE("rk mpi sys init fail!");
    }

    // venc init
    RK_CODEC_ID_E enCodecType = RK_VIDEO_ID_AVC;
    venc_init(0, encWidth, encHeight, enCodecType);
}

VideoEncoder::~VideoEncoder()
{
    if (stFrame.pstPack) {
        free(stFrame.pstPack);
        stFrame.pstPack = nullptr;
    }
}


void VideoEncoder::do_save_video(void* mbBlk, uint64_t pts)
{
    // 零拷贝：直接使用 CameraManager 传来的 DMA buffer
    h264_frame.stVFrame.pMbBlk = reinterpret_cast<MB_BLK>(mbBlk);
    h264_frame.stVFrame.u64PTS = pts;

    /**************************
     * 向VENC发送原始图像进行编码
     * 0为编码通道号
     * h264_frame 为原始图像信息
     * 1000 表示超时1000ms，避免硬件卡死导致线程永久阻塞
     **************************/
    RK_S32 s32Ret = RK_MPI_VENC_SendFrame(0, &h264_frame, 1000);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("RK_MPI_VENC_SendFrame fail %x", s32Ret);
        return;
    }

    // 编码器已持有帧引用，通知 CameraManager 释放 VI 帧
    emit sigReleaseViFrame(mbBlk);

    /**************************
     * 获取编码码流
     * 0为编码通道号
     * stFrame为码流结构体指针
     * 1000 表示超时1000ms
     **************************/
    s32Ret = RK_MPI_VENC_GetStream(0, &stFrame, 1000);

    if(s32Ret == RK_SUCCESS) {
        emit sigVideo2Mp4Mux(&stFrame);
    }

    s32Ret = RK_MPI_VENC_ReleaseStream(0, &stFrame);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("RK_MPI_VENC_ReleaseStream fail %x", s32Ret);
    }
}
