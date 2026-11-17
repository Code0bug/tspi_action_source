#ifndef MEDIA_SERVICE_H
#define MEDIA_SERVICE_H

/*
 * UI 与 media-service 之间的共享 D-Bus 接口（system bus）。
 * 统一定义服务标识及各业务接口，后续可扩展音频、录像、播放等功能。
 *
 * 当前相机采集方法：
 *   StartCapture() -> b   开启采集；返回调用后是否处于采集状态
 *   StopCapture()  -> b   暂停采集；返回调用后是否处于采集状态
 *
 * 服务端发出的信号：
 *   RecordStarted()
 *   RecordStopped()
 *   PhotoSaved(s path)
 */
#define MEDIA_SERVICE_BUS_NAME     "com.tspi.MediaService"
#define MEDIA_SERVICE_OBJECT_PATH  "/com/tspi/MediaService"
#define MEDIA_SERVICE_INTERFACE    "com.tspi.MediaService"

#define MEDIA_SERVICE_METHOD_START_CAPTURE "StartCapture"
#define MEDIA_SERVICE_METHOD_STOP_CAPTURE  "StopCapture"

#define MEDIA_SERVICE_SIGNAL_RECORD_STARTED "RecordStarted"
#define MEDIA_SERVICE_SIGNAL_RECORD_STOPPED "RecordStopped"
#define MEDIA_SERVICE_SIGNAL_PHOTO_SAVED    "PhotoSaved"

#endif
