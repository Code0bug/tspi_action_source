#include "cameramanager.h"

CameraManager::CameraManager()
{

    // rkmpi init
    if (RK_MPI_SYS_Init() != RK_SUCCESS) {
        RK_LOGE("rk mpi sys init fail!");
        // return -1;
    }

    isRunning = 0;
    photo_flag = 0;
    video_flag = 0;
    write_idx_ = 0;

    // 分配 BGR 输出 DMA 双缓冲（用于零拷贝录像）
    RK_U32 bgr_frame_size = 1920 * 1080 * 3;  // BGR888
    for (int i = 0; i < 2; i++) {
        bgr_blk_[i] = MB_INVALID_HANDLE;
        bgr_fd_[i] = -1;
        RK_S32 ret = RK_MPI_MMZ_Alloc(&bgr_blk_[i], bgr_frame_size, RK_MMZ_ALLOC_TYPE_IOMMU);
        if (ret != RK_SUCCESS) {
            RK_LOGE("BGR DMA buffer %d alloc failed: %x", i, ret);
            continue;
        }
        bgr_fd_[i] = RK_MPI_MMZ_Handle2Fd(bgr_blk_[i]);
        if (bgr_fd_[i] < 0) {
            RK_LOGE("BGR DMA buffer %d get fd failed: %x", i, bgr_fd_[i]);
            RK_MPI_MMZ_Free(bgr_blk_[i]);
            bgr_blk_[i] = MB_INVALID_HANDLE;
        }
    }

}

CameraManager::~CameraManager()
{
    // 释放 BGR DMA 双缓冲
    for (int i = 0; i < 2; i++) {
        if (bgr_blk_[i] != MB_INVALID_HANDLE) {
            RK_MPI_MMZ_Free(bgr_blk_[i]);
            bgr_blk_[i] = MB_INVALID_HANDLE;
            bgr_fd_[i] = -1;
        }
    }

    RK_MPI_VI_DisableChn(0, 0);
    RK_MPI_VI_DisableChn(0, 1);
    RK_MPI_VI_DisableDev(0);
    RK_MPI_SYS_Exit();
}


void CameraManager::startCapture()
{
    // vi init
    vi_dev_init();

    vi_chn_init(0, 3264, 2448);   // channel 0 最大支持8M  3264*2448
    vi_chn_init(1, 1920, 1080);   // channel 1 最大支持1920*1080 


    isRunning = 1;
    while (isRunning && !QThread::currentThread()->isInterruptionRequested()) {
        // 视频图像帧信息结构体，存储采集视频帧
        VIDEO_FRAME_INFO_S stViFrame;
        // get vi frame
        RK_S32 s32Ret = RK_MPI_VI_GetChnFrame(0, 1, &stViFrame, 1000);


        if(s32Ret == RK_SUCCESS) {

            // printf("Camera PTS = %ld\r\n", stViFrame.stVFrame.u64PTS);

            unsigned char* dat = reinterpret_cast<unsigned char*>(RK_MPI_MB_Handle2VirAddr(stViFrame.stVFrame.pMbBlk));

            // 转换为RGB888格式
            int height = stViFrame.stVFrame.u32Height;
            int width = stViFrame.stVFrame.u32Width;

            cv::Mat ui_frame(368, 552, CV_8UC3);

            // 利用rga进行像素格式转换（预览路径，仍用 virtualaddr）
            rga_buffer_t src_img = wrapbuffer_virtualaddr(dat, width, height, RK_FORMAT_YCrCb_420_SP);
            rga_buffer_t dst_img = wrapbuffer_virtualaddr(ui_frame.data, 552, 368, RK_FORMAT_RGB_888);
            imresize(src_img, dst_img);
            imcvtcolor(src_img, dst_img, RK_FORMAT_YCrCb_420_SP, RK_FORMAT_RGB_888);

            if (video_flag) { // 录像

                if (first_video_frame) { // 如果是录像的第一帧，取其作为封面
                    first_video_frame = 0;

                    QDateTime current = QDateTime::currentDateTime();
                    QString fileName = current.toString("yyyyMMdd_hhmmss");
                    // 录像保存路径
                    QString savePath_mp4 = "/userdata/video/" + fileName + ".mp4";

                    // 将封面图像保存
                    QString first_frame_savePath = "/userdata/firstFrame/" + fileName + ".jpg";
                    cv::imwrite(first_frame_savePath.toStdString(), ui_frame);
                    // 强制刷盘，保证数据写入 eMMC
                    int fd = open(first_frame_savePath.toStdString().c_str(), O_RDONLY);
                    fsync(fd);
                    close(fd);

                    emit videoFinishedToUpdate(savePath_mp4, ui_frame); // 更新录像列表
                    emit videoFinishedToIMURecord(fileName);  // 将开始录像的时间戳发送给imu记录文件
                    emit sigVideoMp4Init(savePath_mp4);       // MP4初始化
                }

                // 零拷贝 RGA：NV12 → BGR888，直接写入 DMA 双缓冲
                int idx = write_idx_;
                write_idx_ = 1 - write_idx_;

                RK_S32 src_fd = RK_MPI_MB_Handle2Fd(stViFrame.stVFrame.pMbBlk);
                if (src_fd >= 0 && bgr_blk_[idx] != MB_INVALID_HANDLE) {
                    rga_buffer_t src_img_video = wrapbuffer_fd(src_fd, width, height, RK_FORMAT_YCrCb_420_SP);
                    rga_buffer_t dst_img_video = wrapbuffer_fd(bgr_fd_[idx], 1920, 1080, RK_FORMAT_BGR_888);
                    IM_STATUS rga_ret = imcvtcolor(src_img_video, dst_img_video, RK_FORMAT_YCrCb_420_SP, RK_FORMAT_BGR_888);
                    if (rga_ret == IM_STATUS_SUCCESS) {
                        capFinishedToVideo(reinterpret_cast<void*>(bgr_blk_[idx]), stViFrame.stVFrame.u64PTS);
                    } else {
                        RK_LOGE("RGA imcvtcolor(recording) failed: %d", rga_ret);
                    }
                }
            }

            emit capFinishedToUI(ui_frame);

            s32Ret = RK_MPI_VI_ReleaseChnFrame(0, 1, &stViFrame);
            if (s32Ret != RK_SUCCESS) {
                RK_LOGE("RK_MPI_VI_ReleaseChnFrame fail %x", s32Ret);
            }
        }
    }

}


void CameraManager::takePhoto()
{
    const int DROP_FRAMES = 5; // 丢掉前几帧
    int dropped = 0;

    while (1) {
        VIDEO_FRAME_INFO_S stViFrame;
        RK_S32 s32Ret;

        s32Ret = RK_MPI_VI_GetChnFrame(0, 0, &stViFrame, -1);
        if (s32Ret != RK_SUCCESS) {
            continue;
        }

        if (dropped < DROP_FRAMES) {
            // 丢掉前几帧
            RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
            dropped++;
            continue;
        }

        unsigned char* dat = reinterpret_cast<unsigned char*>(RK_MPI_MB_Handle2VirAddr(stViFrame.stVFrame.pMbBlk));
        int height = stViFrame.stVFrame.u32Height;
        int width = stViFrame.stVFrame.u32Width;

        cv::Mat frame(height, width, CV_8UC3);

        // 利用rga进行像素格式转换
        rga_buffer_t src_img = wrapbuffer_virtualaddr(dat, width, height, RK_FORMAT_YCrCb_420_SP);
        rga_buffer_t dst_img = wrapbuffer_virtualaddr(frame.data, width, height, RK_FORMAT_RGB_888);
        imcvtcolor(src_img, dst_img, RK_FORMAT_YCrCb_420_SP, RK_FORMAT_RGB_888);

        if (!frame.empty()) {
            // 保存图片
            QDateTime current = QDateTime::currentDateTime();
            QString fileName = current.toString("yyyyMMdd_hhmmss") + ".bmp";
            // 拼接保存路径
            QString savePath = "/userdata/picture/" + fileName;

            // 保存图片
//            cv::imwrite(savePath.toStdString(), frame);
//            if (!saveImageSafe(savePath, frame)) {
//                fprintf(stderr, "save image failed: %s\n", savePath.toStdString().c_str());
//            }
//            else {
//                printf("taking photo: %s\n", savePath.toStdString().c_str());
//            }

            emit photoFinishedToSave(savePath, frame); // 照片保存

        }

        // 释放帧
        s32Ret = RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("RK_MPI_VI_ReleaseChnFrame fail %x", s32Ret);
        }

        break; // 只拍一张有效照片
    }
}


void CameraManager::stopCapture()
{
    isRunning = 0;
    RK_MPI_VI_DisableChn(0, 0);
    RK_MPI_VI_DisableChn(0, 1);
    RK_MPI_VI_DisableDev(0);

    RK_MPI_SYS_Exit();
}

void CameraManager::startVideo()
{

    video_flag = 1;
    first_video_frame = 1;

}

void CameraManager::stopVideo()
{
    video_flag = 0;
    emit videoFinishedToMP4();
}


