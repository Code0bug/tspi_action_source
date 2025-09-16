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
//    vi_chn_init(0, 3264, 2448);
    vi_chn_init(0, 3264, 2448); // channel 0 最大支持8M
    vi_chn_init(1, 640, 640); // channel 1 最大支持1920*1080

//    std::cout << cv::getBuildInformation() << std::endl;
    isRunning = 0;
    detect_status = 0;
    photo_flag = 0;

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
            cv::Mat yuv420sp(height + height / 2, width, CV_8UC1, dat);
            cv::Mat frame(height, width, CV_8UC3);


            cv::cvtColor(yuv420sp, frame, cv::COLOR_YUV420sp2BGR); // COLOR_YUV2RGB_NV12

            emit capFinishedToUI(frame);

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
    detect_status = 0;
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
        cv::cvtColor(yuv420sp, frame, cv::COLOR_YUV420sp2BGR);

        if (!frame.empty()) {
            // 保存图片
            QDateTime current = QDateTime::currentDateTime();
            QString fileName = current.toString("yyyyMMdd_hhmmss") + ".jpg";
            QByteArray ba = fileName.toLocal8Bit();
            const char* c_fileName = ba.constData();

            image_buffer_t src_image;
            src_image.width = width;
            src_image.height = height;
            src_image.format = IMAGE_FORMAT_RGB888;
            src_image.virt_addr = frame.data;
            src_image.size = RK_MPI_MB_GetSize(stViFrame.stVFrame.pMbBlk);
            src_image.fd = RK_MPI_MB_Handle2Fd(stViFrame.stVFrame.pMbBlk);

            cv::imwrite("/root/img.jpg", frame);

            write_image(c_fileName, &src_image);
            printf("taking photo: %s\n", c_fileName);
        }

        // 释放帧
        s32Ret = RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("RK_MPI_VI_ReleaseChnFrame fail %x", s32Ret);
        }

        break; // 只拍一张有效照片
    }
}

//void CameraCapture::takePhoto()
//{

//    while (1) {
//        // 视频图像帧信息结构体，存储采集视频帧
//        VIDEO_FRAME_INFO_S stViFrame;
//        RK_S32 s32Ret;
//        // get vi frame
//        s32Ret = RK_MPI_VI_GetChnFrame(0, 0, &stViFrame, -1);
//        if(s32Ret == RK_SUCCESS) {

//            unsigned char* dat = reinterpret_cast<unsigned char*>(RK_MPI_MB_Handle2VirAddr(stViFrame.stVFrame.pMbBlk));

//            // 转换为RGB888格式
//            int height = stViFrame.stVFrame.u32Height;
//            int width = stViFrame.stVFrame.u32Width;
//            cv::Mat yuv420sp(height + height / 2, width, CV_8UC1, dat);
//            cv::Mat frame(height, width, CV_8UC3);


//            cv::cvtColor(yuv420sp, frame, cv::COLOR_YUV420sp2BGR); // COLOR_YUV2RGB_NV12

//            if (frame.empty()) {
//                printf("frame is empty\n");
//            } else {

//                // 获取当前时间
//                QDateTime current = QDateTime::currentDateTime();

//                // 格式化为字符串，例如：20250915_134523.jpg
//                QString fileName = current.toString("yyyyMMdd_hhmmss") + ".jpg";
//                // 转换为 const char*
//                QByteArray ba = fileName.toLocal8Bit();   // 保证生命周期
//                const char* c_fileName = ba.constData();

//                image_buffer_t src_image;
//                src_image.width = width;
//                src_image.height = height;
//                src_image.format = IMAGE_FORMAT_RGB888;
//                src_image.virt_addr = frame.data;
//                src_image.size = RK_MPI_MB_GetSize(stViFrame.stVFrame.pMbBlk);
//                src_image.fd = RK_MPI_MB_Handle2Fd(stViFrame.stVFrame.pMbBlk);



//                static int i = 0;
//                i++;
//                if (i > 6) {
//                    write_image(c_fileName, &src_image);
//                    printf("taking photo!\n");

//                    s32Ret = RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
//                    if (s32Ret != RK_SUCCESS) {
//                        RK_LOGE("RK_MPI_VI_ReleaseChnFrame fail %x", s32Ret);
//                    }
//                    break;
//                }

//            }


//        }

//        s32Ret = RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
//        if (s32Ret != RK_SUCCESS) {
//            RK_LOGE("RK_MPI_VI_ReleaseChnFrame fail %x", s32Ret);
//        }
//    }


//}

