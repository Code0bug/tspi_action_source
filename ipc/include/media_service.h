#ifndef MEDIA_SERVICE_H
#define MEDIA_SERVICE_H

/*
 * UI 与 media-service 之间的共享 D-Bus 接口（system bus）。
 *
 * UI -> MediaService：
 *   StartCapture() : 切回预览界面后，开启采集
 *   StopCapture()  : 离开预览界面后，停止采集
 *   PlayVideo(s path) : 点击录像列表视频项，播放指定视频
 *   StopVideo()    : 退出播放页，停止播放
 *
 * MediaService -> UI：
 *   RecordStarted()      : 开始录像，UI 切到录像计时页
 *   RecordStopped()      : 停止录像，UI 回到预览页
 *   PhotoStarted()       : 开始拍照，UI 右上角显示拍摄中
 *   PhotoSaved(s path)   : 拍照完成，关掉提示；path 非空则插入相册
 *   VideoSaved(s path)   : 录像完成，更新录像列表
 */
#define MEDIA_SERVICE_BUS_NAME     "com.tspi.MediaService"
#define MEDIA_SERVICE_OBJECT_PATH  "/com/tspi/MediaService"
#define MEDIA_SERVICE_INTERFACE    "com.tspi.MediaService"

#define MEDIA_SERVICE_METHOD_START_CAPTURE "StartCapture"
#define MEDIA_SERVICE_METHOD_STOP_CAPTURE  "StopCapture"
#define MEDIA_SERVICE_METHOD_PLAY_VIDEO    "PlayVideo"
#define MEDIA_SERVICE_METHOD_STOP_VIDEO    "StopVideo"

#define MEDIA_SERVICE_SIGNAL_RECORD_STARTED "RecordStarted"
#define MEDIA_SERVICE_SIGNAL_RECORD_STOPPED "RecordStopped"
#define MEDIA_SERVICE_SIGNAL_PHOTO_STARTED  "PhotoStarted"
#define MEDIA_SERVICE_SIGNAL_PHOTO_SAVED    "PhotoSaved"
#define MEDIA_SERVICE_SIGNAL_VIDEO_SAVED    "VideoSaved"

#endif
