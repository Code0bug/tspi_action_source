#include "videodecget.h"


RK_S32 mpi_vdec_poll_event(RK_S32 timeoutMsec, RK_S32 fd) {
    RK_S32 num_fds = 1;
    struct pollfd pollFds[num_fds];
    RK_S32 ret = 0;

    RK_ASSERT(fd > 0);
    memset(pollFds, 0, sizeof(pollFds));
    pollFds[0].fd = fd;
    pollFds[0].events = (POLLPRI | POLLIN | POLLERR | POLLNVAL | POLLHUP);

    ret = poll(pollFds, num_fds, timeoutMsec);
    if (ret > 0 && (pollFds[0].revents & (POLLERR | POLLNVAL | POLLHUP))) {
        RK_LOGE("fd:%d polled error", fd);
        return -1;
    }

    return ret;
}

VideoDecGet::VideoDecGet(QObject *parent) : QObject(parent)
{

}

void VideoDecGet::do_get_frame()
{
    VIDEO_FRAME_INFO_S sFrame;

    RK_S32 vdec_fd = RK_MPI_VDEC_GetFd(0);
    if (vdec_fd <= 0) {
        printf("get fd chn failed \n");
    }

    RK_S32 s32Ret;

    stop_get_frame = 0;

    memset(&sFrame, 0, sizeof(VIDEO_FRAME_INFO_S));

    while (!stop_get_frame) {

        if (vdec_fd > 0) {
            s32Ret = mpi_vdec_poll_event(-1, vdec_fd);
            if (s32Ret < 0) {
                usleep(1000llu);
                continue;
            } else {
                printf("poll success!\n");
            }
        }

        s32Ret = RK_MPI_VDEC_GetFrame(0, &sFrame, 20);
        if (s32Ret >= 0) {

            if ((sFrame.stVFrame.u32FrameFlag & FRAME_FLAG_SNAP_END) == FRAME_FLAG_SNAP_END) { // 最后一帧
                RK_MPI_VDEC_ReleaseFrame(0, &sFrame);
                break;
            }

            unsigned char* dat = reinterpret_cast<unsigned char*>(RK_MPI_MB_Handle2VirAddr(sFrame.stVFrame.pMbBlk));

            int height = sFrame.stVFrame.u32Height;
            int width = sFrame.stVFrame.u32Width;

            // 送入rga的源码图像长宽必须16位字节对齐，即width%16=0，否则图像会出现绿边
            if (height % 16 != 0) {
                while (height % 16 != 0) {
                    height++;
                }
            }
            if (width % 16 != 0) {
                while (width % 16 != 0) {
                    width++;
                }
            }

            cv::Mat video_frame_rgb(368, 552, CV_8UC3);

            // 利用rga进行像素格式转换
            rga_buffer_t src_img = wrapbuffer_virtualaddr(dat, width, height, RK_FORMAT_YCrCb_420_SP);
            rga_buffer_t dst_img = wrapbuffer_virtualaddr(video_frame_rgb.data, 552, 368, RK_FORMAT_BGR_888);
            imresize(src_img, dst_img);
            imcvtcolor(src_img, dst_img, RK_FORMAT_YCrCb_420_SP, RK_FORMAT_BGR_888);

            emit VideodecToUI(video_frame_rgb);

            printf("vdec get frame success! format : %d\n", sFrame.stVFrame.enPixelFormat);

//            cv::imwrite("/root/test.jpg", video_frame_rgb);

            RK_MPI_VDEC_ReleaseFrame(0, &sFrame);

            // MP4 播放时发送端已按容器时间戳节流，这里不再固定 sleep，避免视频时钟跑偏。

        } else {
            printf("get frame failed! 0x%08X\n", s32Ret);
            usleep(1000llu);
        }
    }

    RK_MPI_VDEC_StopRecvStream(0);

    RK_MPI_VDEC_CloseFd(0);
    RK_MPI_VDEC_DestroyChn(0);
}

void VideoDecGet::do_stop_get_frame()
{
    stop_get_frame = 1;
}
