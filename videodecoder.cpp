#include "videodecoder.h"


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

VideoDecoder::VideoDecoder(QObject *parent) : QObject(parent),
    stop_get_frame(false), vdecReady(false)
{
}

VideoDecoder::~VideoDecoder()
{
    if (vdecReady) {
        destroyVdec();
    }
}

void VideoDecoder::destroyVdec()
{
    if (!vdecReady.exchange(false)) {
        return;
    }
    RK_MPI_VDEC_StopRecvStream(0);
    RK_MPI_VDEC_CloseFd(0);
    RK_MPI_VDEC_DestroyChn(0);
}

void VideoDecoder::reinitVdec()
{
    // 先销毁旧通道（若存在），再重新创建
    if (vdecReady) {
        destroyVdec();
    }
    if (vdec_init(1920, 1080) == 0) {
        vdecReady = true;
    } else {
        printf("VideoDecoder::reinitVdec: vdec_init failed\n");
    }
}

void VideoDecoder::do_get_frame()
{
    // 在 worker 线程内完成 VDEC 通道的创建，避免与 UI 线程的 vdec_init 竞争
    reinitVdec();
    if (!vdecReady) {
        return;
    }

    VIDEO_FRAME_INFO_S sFrame;

    RK_S32 vdec_fd = RK_MPI_VDEC_GetFd(0);
    if (vdec_fd <= 0) {
        printf("get fd chn failed \n");
    }

    RK_S32 s32Ret;

    stop_get_frame = false;

    memset(&sFrame, 0, sizeof(VIDEO_FRAME_INFO_S));

    while (!stop_get_frame) {

        if (vdec_fd > 0) {
            s32Ret = mpi_vdec_poll_event(100, vdec_fd);
            if (s32Ret < 0) {
                usleep(1000llu);
                continue;
            } else if (s32Ret == 0) {
                continue;
            }
        }

        s32Ret = RK_MPI_VDEC_GetFrame(0, &sFrame, 20);
        if (s32Ret >= 0) {

            if ((sFrame.stVFrame.u32FrameFlag & FRAME_FLAG_SNAP_END) == FRAME_FLAG_SNAP_END) {
                RK_MPI_VDEC_ReleaseFrame(0, &sFrame);
                break;
            }

            int width    = sFrame.stVFrame.u32Width;
            int height   = sFrame.stVFrame.u32Height;
            int virWidth = sFrame.stVFrame.u32VirWidth;
            int virHeight = sFrame.stVFrame.u32VirHeight;
            void* key    = reinterpret_cast<void*>(sFrame.stVFrame.pMbBlk);
            int fd       = RK_MPI_MB_Handle2Fd(sFrame.stVFrame.pMbBlk);

            pendingVdecFrames_[key] = sFrame;

            emit VideodecToUI(fd, width, height, key, virWidth, virHeight);

        } else {
            usleep(1000llu);
        }
    }

    // 循环退出：归还所有尚未被 UI 释放的 VDEC 帧
    for (auto it = pendingVdecFrames_.begin(); it != pendingVdecFrames_.end(); ++it) {
        RK_MPI_VDEC_ReleaseFrame(0, &it.value());
    }
    pendingVdecFrames_.clear();

    destroyVdec();
}

void VideoDecoder::releaseVdecFrame(void* mbBlk)
{
    auto it = pendingVdecFrames_.find(mbBlk);
    if (it == pendingVdecFrames_.end()) {
        return;
    }

    RK_MPI_VDEC_ReleaseFrame(0, &it.value());
    pendingVdecFrames_.erase(it);
}

void VideoDecoder::do_stop_get_frame()
{
    stop_get_frame = true;
}