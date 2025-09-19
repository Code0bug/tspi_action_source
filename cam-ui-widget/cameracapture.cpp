#include "cameracapture.h"


CameraCapture::CameraCapture()
{

    // rkmpi init
    if (RK_MPI_SYS_Init() != RK_SUCCESS) {
        RK_LOGE("rk mpi sys init fail!");
        // return -1;
    }

    // vi init
    vi_dev_init();

    vi_chn_init(0, 3264, 2448);   // channel 0 最大支持8M
    vi_chn_init(1, 1920, 1080);   // channel 1 最大支持1920*1080

    isRunning = 0;
    photo_flag = 0;
    video_flag = 0;

}

void CameraCapture::startCapture()
{
    isRunning = 1;
    while (isRunning) {
        // 视频图像帧信息结构体，存储采集视频帧
        VIDEO_FRAME_INFO_S stViFrame;
        // get vi frame
        RK_S32 s32Ret = RK_MPI_VI_GetChnFrame(0, 1, &stViFrame, -1);

        if(s32Ret == RK_SUCCESS) {

            unsigned char* dat = reinterpret_cast<unsigned char*>(RK_MPI_MB_Handle2VirAddr(stViFrame.stVFrame.pMbBlk));

            // 转换为RGB888格式
            int height = stViFrame.stVFrame.u32Height;
            int width = stViFrame.stVFrame.u32Width;

            cv::Mat ui_frame(480, 800, CV_8UC3);
//            cv::Mat ui_frame_rotate(480, 800, CV_8UC3);
            cv::Mat video_frame(height, width, CV_8UC3);

            // 利用rga进行像素格式转换
            rga_buffer_t src_img = wrapbuffer_virtualaddr(dat, width, height, RK_FORMAT_YCrCb_420_SP);
            rga_buffer_t dst_img = wrapbuffer_virtualaddr(ui_frame.data, 800, 480, RK_FORMAT_BGR_888);
//            rga_buffer_t dst_img_rotate = wrapbuffer_virtualaddr(ui_frame_rotate.data, 800, 480, RK_FORMAT_BGR_888);
            imresize(src_img, dst_img);
            imcvtcolor(src_img, dst_img, RK_FORMAT_YCrCb_420_SP, RK_FORMAT_BGR_888);
//            imrotate(dst_img, dst_img_rotate, IM_HAL_TRANSFORM_ROT_180);  // 设置旋转


            if (video_flag) { // 录像
                rga_buffer_t dst_img_video = wrapbuffer_virtualaddr(video_frame.data, width, height, RK_FORMAT_BGR_888);
                imcvtcolor(src_img, dst_img_video, RK_FORMAT_YCrCb_420_SP, RK_FORMAT_BGR_888);
                capFinishedToVideo(video_frame);
            }

            emit capFinishedToUI(ui_frame);




        }

        s32Ret = RK_MPI_VI_ReleaseChnFrame(0, 1, &stViFrame);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("RK_MPI_VI_ReleaseChnFrame fail %x", s32Ret);
        }
    }

}


void CameraCapture::stopCapture()
{
    isRunning = 0;
}

void CameraCapture::startVideo()
{
    video_flag = 1;
}

void CameraCapture::stopVideo()
{
    video_flag = 0;
}

void CameraCapture::takePhoto()
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
            QString fileName = current.toString("yyyyMMdd_hhmmss") + ".jpg";
            // 拼接保存路径
            QString savePath = "/userdata/picture/" + fileName;

            // 保存图片
            cv::imwrite(savePath.toStdString(), frame);

            printf("taking photo: %s\n", savePath.toStdString().c_str());
        }

        // 释放帧
        s32Ret = RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("RK_MPI_VI_ReleaseChnFrame fail %x", s32Ret);
        }

        break; // 只拍一张有效照片
    }
}



