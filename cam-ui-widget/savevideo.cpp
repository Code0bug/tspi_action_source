#include "savevideo.h"

SaveVideo::SaveVideo()
{

    RK_S32 s32Ret = 0;

    stFrame.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S));

    data = new unsigned char;
    // 内存缓存池配置
    MB_POOL_CONFIG_S PoolCfg;
    memset(&PoolCfg, 0, sizeof(MB_POOL_CONFIG_S));
    PoolCfg.u64MBSize = encWidth * encHeight * 3 ;  		   // 缓存块大小
    PoolCfg.u32MBCnt = 1;                              // 内存缓存池中缓存块个数
    PoolCfg.enAllocType = MB_ALLOC_TYPE_DMA;           // 申请内存类型
    //PoolCfg.bPreAlloc = RK_FALSE;
    MB_POOL src_Pool = RK_MPI_MB_CreatePool(&PoolCfg); // 创建内存缓存池
    printf("Create Pool success !\n");

    // 从内存缓存池中获取一个缓存块，返回该缓存块的地址
    MB_BLK src_Blk = RK_MPI_MB_GetMB(src_Pool, encWidth * encHeight * 3, RK_TRUE);

    // 配置编码帧信息
    h264_frame.stVFrame.u32Width = encWidth;
    h264_frame.stVFrame.u32Height = encHeight;
    h264_frame.stVFrame.u32VirWidth = encWidth;
    h264_frame.stVFrame.u32VirHeight = encHeight;
    h264_frame.stVFrame.enPixelFormat =  RK_FMT_RGB888;
    h264_frame.stVFrame.u32FrameFlag = 160;
    h264_frame.stVFrame.pMbBlk = src_Blk;   // 将帧地址关联到缓存块
    // 将缓存块地址转为虚拟地址，并创建Mat类型变量指向该地址
    data = (unsigned char *)RK_MPI_MB_Handle2VirAddr(src_Blk);


    // rkmpi init
    if (RK_MPI_SYS_Init() != RK_SUCCESS) {
        RK_LOGE("rk mpi sys init fail!");
        // return -1;
    }

    // venc init
    RK_CODEC_ID_E enCodecType = RK_VIDEO_ID_AVC;
    venc_init(0, encWidth, encHeight, enCodecType);

    fp = fopen("/userdata/video/1.bin", "wb");
    if (!fp) {
        perror("fopen");
    }
}

void SaveVideo::do_save_video(cv::Mat buf)
{
    printf("loc---------------------------\n");
    memcpy(data, buf.data, encWidth * encHeight * 3);
    printf("loc1---------------------------\n");
    /**************************
     * 向VENC发送原始图像进行编码
     * 0为编码通道号
     * h264_frame 为原始图像信息
     * -1 表示阻塞，发送成功后释放
     **************************/
    RK_MPI_VENC_SendFrame(0, &h264_frame, -1);

    /**************************
     * 获取编码码流
     * 0为编码通道号
     * stFrame为码流结构体指针
     * -1 表示阻塞，获取编码流后释放
     **************************/
    RK_S32 s32Ret = RK_MPI_VENC_GetStream(0, &stFrame, -1);

    if(s32Ret == RK_SUCCESS) {

        void *pData = RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk); // 编码帧数据
        uint32_t len = stFrame.pstPack->u32Len;

        // 保存录像
        if (fp) {
            size_t written = fwrite(pData, 1, len, fp);
            if (written != len) {
                fprintf(stderr, "fwrite failed, written=%zu, len=%u\n", written, len);
            }
            fflush(fp); // 保证写入磁盘
        }
    }

    s32Ret = RK_MPI_VENC_ReleaseStream(0, &stFrame);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("RK_MPI_VENC_ReleaseStream fail %x", s32Ret);
    }

}


