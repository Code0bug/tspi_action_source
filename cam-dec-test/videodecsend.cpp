#include "videodecsend.h"

static RK_S32 mpi_vdec_free(void *opaque) {
    if (opaque)
        free(opaque);
    return 0;
}

VideoDecSend::VideoDecSend(QObject *parent) : QObject(parent)
{

}

void VideoDecSend::do_send_frame()
{
    QString path = "/userdata/video/20170804_092349.bin";

    MB_EXT_CONFIG_S pstMbExtConfig;
    MB_BLK buffer = RK_NULL;
    VDEC_STREAM_S stStream;
    RK_S32 s32ReachEOS = 0;
    RK_S32 s32Ret = 0;
    RK_U8 *data;

    memset(&stStream, 0, sizeof(VDEC_STREAM_S));


    FILE* fp = fopen(path.toStdString().c_str(), "r");
    if (fp == RK_NULL) {
        printf("fopen failed!\n");
    }

    while(!s32ReachEOS) {
        data = reinterpret_cast<RK_U8 *>(calloc(1024, sizeof(RK_U8)));
        memset(data, 0, 1024);
        RK_S32 s32Size = fread(data, 1, 1024, fp);
        printf("s32Size = %d\n", s32Size);
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


}
