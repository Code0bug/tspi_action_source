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
    vi_chn_init(0, 640, 640);


    isRunning = 0;
    detect_status = 0;

}

void CameraCapture::startCapture()
{
    isRunning = 1;
    while (isRunning) {
        // 视频图像帧信息结构体，存储采集视频帧
        VIDEO_FRAME_INFO_S stViFrame;
        // get vi frame
        RK_S32 s32Ret = RK_MPI_VI_GetChnFrame(0, 0, &stViFrame, -1);
        if(s32Ret == RK_SUCCESS) {

            image_buffer_t src_image;
            src_image.width = stViFrame.stVFrame.u32Width;
            src_image.height = stViFrame.stVFrame.u32Height;
            src_image.format = IMAGE_FORMAT_YUV420SP_NV12;
            src_image.virt_addr = reinterpret_cast<unsigned char*>(RK_MPI_MB_Handle2VirAddr(stViFrame.stVFrame.pMbBlk));
            src_image.size = RK_MPI_MB_GetSize(stViFrame.stVFrame.pMbBlk);
            src_image.fd = RK_MPI_MB_Handle2Fd(stViFrame.stVFrame.pMbBlk);

            std::cout << "get frame!" << std::endl;

            if (detect_status == 1) {
                emit capFinishedToRKNN(src_image);
            } else {
                // 转换为RGB888格式
                int height = src_image.height;
                int width = src_image.width;
                cv::Mat yuv420sp(height + height / 2, width, CV_8UC1, src_image.virt_addr);
                cv::Mat frame(height, width, CV_8UC3);
                cv::cvtColor(yuv420sp, frame, cv::COLOR_YUV420sp2BGR); // COLOR_YUV2RGB_NV12
                emit capFinishedToUI(frame);
            }


        }

        s32Ret = RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
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

void CameraCapture::startDetect()
{
    detect_status = 1;
}

void CameraCapture::stopDetect()
{
    detect_status = 0;
}
