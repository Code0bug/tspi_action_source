#ifndef CAMERA_IPC_H
#define CAMERA_IPC_H

/*
 * UI 与 media-service 之间的相机控制 D-Bus 接口（system bus）。
 *
 * 方法：
 *   StartCapture() -> b   开启采集；返回调用后是否处于采集状态
 *   StopCapture()  -> b   暂停采集；返回调用后是否处于采集状态
 *   IsCapturing()  -> b   查询当前是否采集
 * 信号：
 *   CaptureStateChanged(b capturing)   采集状态变化时由服务发出
 */
#define CAMERA_IPC_BUS_NAME     "com.tspi.MediaService"
#define CAMERA_IPC_OBJECT_PATH  "/com/tspi/MediaService/Camera"
#define CAMERA_IPC_INTERFACE    "com.tspi.MediaService.Camera"

#define CAMERA_IPC_METHOD_START_CAPTURE "StartCapture"
#define CAMERA_IPC_METHOD_STOP_CAPTURE  "StopCapture"
#define CAMERA_IPC_METHOD_IS_CAPTURING  "IsCapturing"
#define CAMERA_IPC_SIGNAL_STATE_CHANGED "CaptureStateChanged"

#endif
