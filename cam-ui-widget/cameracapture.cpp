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

    int ret = vpss_init(0, 1920, 1080);
    if (ret != 0) {
        printf("vpss error!\n");
    }

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
        RK_S32 s32Ret = RK_MPI_VPSS_GetChnFrame(0, 0, &stViFrame, -1);

        if(s32Ret == RK_SUCCESS) {

            unsigned char* dat = reinterpret_cast<unsigned char*>(RK_MPI_MB_Handle2VirAddr(stViFrame.stVFrame.pMbBlk));

            // 转换为RGB888格式
            int height = stViFrame.stVFrame.u32Height;
            int width = stViFrame.stVFrame.u32Width;
//            cv::Mat yuv420sp(height + height / 2, width, CV_8UC1, dat);
//            cv::Mat frame(height, width, CV_8UC3);
            printf("==============%d\n", stViFrame.stVFrame.enPixelFormat);
            printf("==============height: %d  width: %d\n", height, width);
            printf("%d %d %d \n", dat[0], dat[1], dat[2]);
            cv::Mat frame(height, width, CV_8UC3, dat);

//             记录起始时间
//            auto t1 = std::chrono::high_resolution_clock::now();

//            cv::cvtColor(yuv420sp, frame, cv::COLOR_YUV420sp2BGR); // COLOR_YUV2RGB_NV12 这个太耗时了

//            // 记录结束时间
//            auto t2 = std::chrono::high_resolution_clock::now();
//            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

//            std::cout << "cvtColor耗时: " << duration << " us" << std::endl;

            emit capFinishedToUI(frame);

            if (video_flag) { // 录像
//                capFinishedToVideo(frame);
            }


        }

        s32Ret = RK_MPI_VPSS_ReleaseChnFrame(0, 0, &stViFrame);
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

        cv::Mat yuv420sp(height + height / 2, width, CV_8UC1, dat);
        cv::Mat frame(height, width, CV_8UC3);
        cv::cvtColor(yuv420sp, frame, cv::COLOR_YUV420sp2RGB);

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



