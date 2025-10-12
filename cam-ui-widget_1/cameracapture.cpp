#include "cameracapture.h"


CameraCapture::CameraCapture()
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

CameraCapture::~CameraCapture()
{

    RK_MPI_VI_DisableChn(0, 0);
    RK_MPI_VI_DisableChn(0, 1);
    RK_MPI_VI_DisableDev(0);
    RK_MPI_SYS_Exit();
}
int getFrame(int chn, unsigned char *dat)
 {
     // 视频图像帧信息结构体，存储采集视频帧
     VIDEO_FRAME_INFO_S stViFrame;
     // get vi frame
     RK_S32 s32Ret = RK_MPI_VI_GetChnFrame(0, chn, &stViFrame, -1);

     if(s32Ret == RK_SUCCESS) {

         dat = reinterpret_cast<unsigned char*>(RK_MPI_MB_Handle2VirAddr(stViFrame.stVFrame.pMbBlk));

         // 转换为RGB888格式
         int height = stViFrame.stVFrame.u32Height;
         int width = stViFrame.stVFrame.u32Width;

     }

//     s32Ret = RK_MPI_VI_ReleaseChnFrame(0, chn, &stViFrame);
//     if (s32Ret != RK_SUCCESS) {
//         RK_LOGE("RK_MPI_VI_ReleaseChnFrame fail %x", s32Ret);
//     }
 }

void CameraCapture::startCapture()
{
    viInit();

    isRunning = 1;
    while (isRunning) {

        cv::Mat frame(1080, 1920, CV_8UC3);
        cv::Mat ui_frame(480, 800, CV_8UC3);

        getFrame(1, frame.data);

        // 利用rga进行像素格式转换
        rga_buffer_t src_img = wrapbuffer_virtualaddr(frame.data, 1920, 1080, RK_FORMAT_YCrCb_420_SP);
        rga_buffer_t dst_img = wrapbuffer_virtualaddr(ui_frame.data, 800, 480, RK_FORMAT_BGR_888);
        imresize(src_img, dst_img);
        imcvtcolor(src_img, dst_img, RK_FORMAT_YCrCb_420_SP, RK_FORMAT_BGR_888);


        if (video_flag) { // 录像
            rga_buffer_t dst_img_video = wrapbuffer_virtualaddr(frame.data, 1920, 1080, RK_FORMAT_BGR_888);
            imcvtcolor(src_img, dst_img_video, RK_FORMAT_YCrCb_420_SP, RK_FORMAT_BGR_888);
            capFinishedToVideo(frame, video_fp);
        }

        emit capFinishedToUI(ui_frame);
    }

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

            emit photoFinishedToUpdate(savePath);  // 每次拍完照片更新相册

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


void CameraCapture::stopCapture()
{
    isRunning = 0;
    RK_MPI_VI_DisableChn(0, 0);
    RK_MPI_VI_DisableChn(0, 1);
    RK_MPI_VI_DisableDev(0);

    RK_MPI_SYS_Exit();
}

void CameraCapture::startVideo()
{
    QDateTime current = QDateTime::currentDateTime();
    QString fileName = current.toString("yyyyMMdd_hhmmss") + ".bin";
    // 拼接保存路径
    QString savePath = "/userdata/video/" + fileName;
    video_fp = fopen(savePath.toStdString().c_str(), "wb");
    if (!video_fp) {
        perror("fopen");
    }

    emit videoFinishedToUpdate(savePath);

    video_flag = 1;

}

void CameraCapture::stopVideo()
{
    video_flag = 0;
}



