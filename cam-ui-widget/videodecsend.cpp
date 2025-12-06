#include "videodecsend.h"

static RK_S32 mpi_vdec_free(void *opaque) {
    if (opaque)
        free(opaque);
    return 0;
}

VideoDecSend::VideoDecSend(QObject *parent) : QObject(parent)
{

}

void VideoDecSend::do_send_frame(QString path)
{
    MB_EXT_CONFIG_S pstMbExtConfig;
    MB_BLK buffer = RK_NULL;
    VDEC_STREAM_S stStream;
    RK_S32 s32ReachEOS = 0;
    RK_S32 s32Ret = 0;
    RK_U8 *data;

    stop_send_frame = 0;

    memset(&stStream, 0, sizeof(VDEC_STREAM_S));

    FILE* fp = fopen(path.toStdString().c_str(), "r");
    if (fp == RK_NULL) {
        printf("fopen failed!\n");
    }

    while(!s32ReachEOS && !stop_send_frame) {
        data = reinterpret_cast<RK_U8 *>(calloc(65536, sizeof(RK_U8)));
        memset(data, 0, 65536);
        RK_S32 s32Size = fread(data, 1, 65536, fp);  // 一次读取65536个字节
        printf("s32Size = %d\n", s32Size);  // 实际读取到的字节数
        if (s32Size <= 0) {
            s32ReachEOS = 1; // 表示文件读完了
        }

//        printf("vedc......\n");

        memset(&pstMbExtConfig, 0, sizeof(MB_EXT_CONFIG_S));
        pstMbExtConfig.pFreeCB = mpi_vdec_free;
        pstMbExtConfig.pOpaque = data;
        pstMbExtConfig.pu8VirAddr = data;
        pstMbExtConfig.u64Size = s32Size;

        RK_MPI_SYS_CreateMB(&buffer, &pstMbExtConfig);

        stStream.u64PTS = 0;
        stStream.pMbBlk = buffer;
        stStream.u32Len = s32Size;
        stStream.bEndOfStream = s32ReachEOS ? RK_TRUE : RK_FALSE;
        stStream.bEndOfFrame = s32ReachEOS ? RK_TRUE : RK_FALSE;
        stStream.bBypassMbBlk = RK_TRUE;

__RETRY:
        s32Ret = RK_MPI_VDEC_SendStream(0, &stStream, 20);
        if (s32Ret < 0) {
            printf("send stream failed! 0x%08X\n", s32Ret);

            usleep(1000llu);
            goto  __RETRY;
        } else {
            printf("send stream success!\n");
        }

        RK_MPI_MB_ReleaseMB(stStream.pMbBlk);

    }

    fclose(fp);

}


void VideoDecSend::do_stop_send_frame()
{
    stop_send_frame = 1;
}
















