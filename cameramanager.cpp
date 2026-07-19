#include "cameramanager.h"

CameraManager::CameraManager()
{

    // rkmpi init
    if (RK_MPI_SYS_Init() != RK_SUCCESS) {
        RK_LOGE("rk mpi sys init fail!");
        // return -1;
    }

    // vi init
    vi_dev_init();
    vi_chn_init(0, 3264, 2448);   // channel 0 最大支持8M  3264*2448
    vi_chn_init(1, 1920, 1080);   // channel 1 最大支持1920*1080 

    isRunning = false;
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

    isRunning = true;
    while (isRunning && !QThread::currentThread()->isInterruptionRequested()) {

        VIDEO_FRAME_INFO_S stViFrame;
        RK_S32 s32Ret = RK_MPI_VI_GetChnFrame(0, 1, &stViFrame, 1000);

        if (s32Ret != RK_SUCCESS) { // 获取帧失败
            continue;
        }

        printf("pendingViFrames_ = %d\r\n", pendingViFrames_.size());

        int height = stViFrame.stVFrame.u32Height;
        int width  = stViFrame.stVFrame.u32Width;

        // 录像首帧仍需要 cv::Mat 写 jpg 缩略图，仅这一帧走 RGA 软件转换
        if (video_flag && first_video_frame) {
            first_video_frame = 0;

            unsigned char* dat = reinterpret_cast<unsigned char*>(
                RK_MPI_MB_Handle2VirAddr(stViFrame.stVFrame.pMbBlk));

            cv::Mat ui_frame(368, 552, CV_8UC3);
            rga_buffer_t src_img = wrapbuffer_virtualaddr(dat, width, height, RK_FORMAT_YCrCb_420_SP);
            rga_buffer_t dst_img = wrapbuffer_virtualaddr(ui_frame.data, 552, 368, RK_FORMAT_RGB_888);
            imresize(src_img, dst_img);
            imcvtcolor(src_img, dst_img, RK_FORMAT_YCrCb_420_SP, RK_FORMAT_RGB_888);

            QDateTime current = QDateTime::currentDateTime();
            QString fileName = current.toString("yyyyMMdd_hhmmss");
            QString savePath_mp4 = "/userdata/video/" + fileName + ".mp4";
            QString first_frame_savePath = "/userdata/firstFrame/" + fileName + ".jpg";
            cv::imwrite(first_frame_savePath.toStdString(), ui_frame);
            int fdJpg = open(first_frame_savePath.toStdString().c_str(), O_RDONLY);
            fsync(fdJpg);
            close(fdJpg);

            emit videoFinishedToUpdate(savePath_mp4, ui_frame);
            emit videoFinishedToIMURecord(fileName);
            emit sigVideoMp4Init(savePath_mp4);
        }

        // 同一帧被两个消费者持有：
        //   - VideoEncoder（videoEncoderThread 异步处理：SendFrame+GetStream+写Mp4）
        //   - VideoGLWidget（GUI 线程 EGLImage 导入渲染）
        // 引用计数 = video_flag ? 2 : 1，两个消费者都发完 sigReleaseViFrame 才真正释放 VI 帧
        void* key = reinterpret_cast<void*>(stViFrame.stVFrame.pMbBlk);
        pendingViFrames_[key] = {stViFrame, video_flag ? 2 : 1};

        // 异步编码：post 到 videoEncoderThread 事件队列，立即返回
        if (video_flag) {
            emit capFinishedToVideo(key, stViFrame.stVFrame.u64PTS);
        }

        // 异步显示：post 到 GUI 线程事件队列，立即返回
        int vi_fd = RK_MPI_MB_Handle2Fd(stViFrame.stVFrame.pMbBlk);
        emit capFinishedToUI(vi_fd, 1920, 1080, key, stViFrame.stVFrame.u64PTS);
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
    isRunning = false;
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

void CameraManager::releaseViFrame(void* mbBlk)
{
    std::lock_guard<std::mutex> lock(pendingMtx_);
    auto it = pendingViFrames_.find(mbBlk);
    if (it == pendingViFrames_.end()) {
        return;
    }

    if (--it.value().refCount > 0) {
        return; // 还有其他消费者未释放
    }

    VIDEO_FRAME_INFO_S stViFrame = it.value().frame;
    pendingViFrames_.erase(it);

    // 引用计数归零，真正释放 VI 通道 1 的帧回内核
    RK_S32 s32Ret = RK_MPI_VI_ReleaseChnFrame(0, 1, &stViFrame);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("releaseViFrame fail %x", s32Ret);
    }
}


