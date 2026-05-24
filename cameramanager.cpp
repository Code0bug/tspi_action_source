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

}

CameraManager::~CameraManager()
{

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
    while (isRunning) {
        // 视频图像帧信息结构体，存储采集视频帧
        VIDEO_FRAME_INFO_S stViFrame;
        // get vi frame
        RK_S32 s32Ret = RK_MPI_VI_GetChnFrame(0, 1, &stViFrame, -1);


        if(s32Ret == RK_SUCCESS) {

            // printf("Camera PTS = %ld\r\n", stViFrame.stVFrame.u64PTS);

            unsigned char* dat = reinterpret_cast<unsigned char*>(RK_MPI_MB_Handle2VirAddr(stViFrame.stVFrame.pMbBlk));

            // 转换为RGB888格式
            int height = stViFrame.stVFrame.u32Height;
            int width = stViFrame.stVFrame.u32Width;

            cv::Mat ui_frame(368, 552, CV_8UC3);
            cv::Mat video_frame(height, width, CV_8UC3);

            // 利用rga进行像素格式转换
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
                    QString savePath = "/userdata/video/" + fileName + ".bin";
                    QString savePath_mp4 = "/userdata/video/" + fileName + ".mp4";
                    video_fp = fopen(savePath.toStdString().c_str(), "wb");
                    if (!video_fp) {
                        perror("fopen");
                    }

                    // 将封面图像保存
                    QString first_frame_savePath = "/userdata/firstFrame/" + fileName + ".jpg";
                    cv::imwrite(first_frame_savePath.toStdString(), ui_frame);
                    // 强制刷盘，保证数据写入 eMMC
                    int fd = open(first_frame_savePath.toStdString().c_str(), O_RDONLY);
                    fsync(fd);
                    close(fd);

                    emit videoFinishedToUpdate(savePath_mp4, ui_frame); // 更新录像列表
                    emit videoFinishedToIMURecord(fileName);  // 将开始录像的时间戳发送给imu记录文件
                    emit sigVideoMp4Init(savePath_mp4);  // MP4初始化
                }

                rga_buffer_t dst_img_video = wrapbuffer_virtualaddr(video_frame.data, width, height, RK_FORMAT_BGR_888);
                imcvtcolor(src_img, dst_img_video, RK_FORMAT_YCrCb_420_SP, RK_FORMAT_BGR_888);
                capFinishedToVideo(video_frame, video_fp, stViFrame.stVFrame.u64PTS);
            }

            emit capFinishedToUI(ui_frame);

        }

        s32Ret = RK_MPI_VI_ReleaseChnFrame(0, 1, &stViFrame);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("RK_MPI_VI_ReleaseChnFrame fail %x", s32Ret);
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


