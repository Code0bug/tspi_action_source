//#include "playvideo.h"

////static RK_S32 mpi_vdec_free(void *opaque) {
////    if (opaque)
////        free(opaque);
////    return 0;
////}

////RK_S32 mpi_vdec_poll_event(RK_S32 timeoutMsec, RK_S32 fd) {
////    RK_S32 num_fds = 1;
////    struct pollfd pollFds[num_fds];
////    RK_S32 ret = 0;

////    RK_ASSERT(fd > 0);
////    memset(pollFds, 0, sizeof(pollFds));
////    pollFds[0].fd = fd;
////    pollFds[0].events = (POLLPRI | POLLIN | POLLERR | POLLNVAL | POLLHUP);

////    ret = poll(pollFds, num_fds, timeoutMsec);
////    if (ret > 0 && (pollFds[0].revents & (POLLERR | POLLNVAL | POLLHUP))) {
////        RK_LOGE("fd:%d polled error", fd);
////        return -1;
////    }

////    return ret;
////}

//PlayVideo::PlayVideo(QObject *parent) : QObject(parent)
//{
//    int ret = vdec_init(1920,1080);
//    if (ret == 0) {
//        printf("vdec init success! \n");
//    }

////    vdec_fd = RK_MPI_VDEC_GetFd(0); // 获取视频解码通道的设备文件句柄，用于查询解码通道数据有产生解码图像
////    if (vdec_fd <= 0) {
////        printf("get fd chn failed \n");
////    }
//}

//void* get_frame(void* arg)
//{

//    VIDEO_FRAME_INFO_S sFrame;

//    RK_S32 vdec_fd = RK_MPI_VDEC_GetFd(0);
//    if (vdec_fd <= 0) {
//        printf("get fd chn failed \n");
//    }

//    RK_S32 s32Ret;


//    memset(&sFrame, 0, sizeof(VIDEO_FRAME_INFO_S));

//    while (1) {


//        if (vdec_fd > 0) {
//            s32Ret = mpi_vdec_poll_event(-1, vdec_fd);
//            if (s32Ret < 0) {
//                usleep(1000llu);
//                continue;
//            } else {
//                printf("poll success!\n");
//            }
//        }

//        s32Ret = RK_MPI_VDEC_GetFrame(0, &sFrame, 20);
//        if (s32Ret >= 0) {
//            unsigned char* dat = reinterpret_cast<unsigned char*>(RK_MPI_MB_Handle2VirAddr(sFrame.stVFrame.pMbBlk));

//            int height = sFrame.stVFrame.u32Height;
//            int width = sFrame.stVFrame.u32Width;
//            cv::Mat video_frame(height, width, CV_8UC3, dat);

////            emit PlayVideoToUI(video_frame);

//            printf("vdec get frame success!\n");

//            cv::imwrite("/root/test.jpg", video_frame);

//            RK_MPI_VDEC_ReleaseFrame(0, &sFrame);
//        } else {
//            printf("get frame failed! 0x%08X\n", s32Ret);
//            usleep(1000llu);
//        }
//    }

//}

//void PlayVideo::do_play_video(QListWidgetItem *item)
//{
//    QString path = item->data(Qt::UserRole).toString();

//    MB_EXT_CONFIG_S pstMbExtConfig;
//    MB_BLK buffer = RK_NULL;
//    VDEC_STREAM_S stStream;
//    RK_S32 s32ReachEOS = 0;
//    RK_S32 s32Ret = 0;

//    pthread_t getPicThread;

//    memset(&stStream, 0, sizeof(VDEC_STREAM_S));


//    fp = fopen(path.toStdString().c_str(), "r");
//    if (fp == RK_NULL) {
//        printf("fopen failed!\n");
//    }

//    pthread_create(&getPicThread, NULL, get_frame, NULL);

//    while(!s32ReachEOS) {
//        data = reinterpret_cast<RK_U8 *>(calloc(1024, sizeof(RK_U8)));
//        memset(data, 0, 1024);
//        RK_S32 s32Size = fread(data, 1, 1024, fp);
//        if (s32Size <= 0) {
//            s32ReachEOS = 1; // 表示文件读完了
//        }

////        printf("vedc......\n");

//        memset(&pstMbExtConfig, 0, sizeof(MB_EXT_CONFIG_S));
//        pstMbExtConfig.pFreeCB = mpi_vdec_free;
//        pstMbExtConfig.pOpaque = data;
//        pstMbExtConfig.pu8VirAddr = data;
//        pstMbExtConfig.u64Size = s32Size;

//        RK_MPI_SYS_CreateMB(&buffer, &pstMbExtConfig);

//        stStream.u64PTS = 0;
//        stStream.pMbBlk = buffer;
//        stStream.u32Len = s32Size;
//        stStream.bEndOfStream = s32ReachEOS ? RK_TRUE : RK_FALSE;
//        stStream.bEndOfFrame = s32ReachEOS ? RK_TRUE : RK_FALSE;
//        stStream.bBypassMbBlk = RK_TRUE;

//__RETRY:
//        s32Ret = RK_MPI_VDEC_SendStream(0, &stStream, 20);
//        if (s32Ret < 0) {
//            printf("send stream failed! 0x%08X\n", s32Ret);

//            usleep(1000llu);
//            goto  __RETRY;
//        } else {
//            printf("send stream success!\n");
//        }

//        RK_MPI_MB_ReleaseMB(stStream.pMbBlk);



//    }





//}
