#include "keycontrol.h"

KeyControl::KeyControl()
{

    fd = open("/dev/input/event4", O_RDWR);
    if (fd < 0) {
        printf("Can't open event file\r\n");
    }

    timer.invalidate();   // 初始化为无效状态

    cam_video_status = 0; // 表示相机录像状态:录像(1)、非录像(0)
}

void KeyControl::key_query()
{
    while (1) {
        int ret = read(fd, &inputevent, sizeof(inputevent));
        if (ret > 0) { // 读取到有效数据
            if (inputevent.type == EV_KEY) {

                if (inputevent.value == 0) {  // 按键按下
                    timer.start();            // 开始计时
                } else if (inputevent.value == 1) { // 按键松开
                    // 松开时判断长按短按
                    if (timer.isValid()) {
                        qint64 elapsed = timer.elapsed(); // 计时时间 ms
                        if (elapsed < 500) {             // 短按
                            if (cam_video_status == 0) {  // 如果当前不在录像状态
                                emit keyToTakephoto();    // 发出拍照信号
//                                printf("short press \n");
                            } else {                      // 如果当前在录像状态
                                emit keyToStopvideo();    // 发出停止录像信号
                                cam_video_status = 0;     // 录像状态置0
                            }
                        } else {                          // 长按
                            emit keyToStartvideo();       // 录像
                            cam_video_status = 1;         // 录像状态置1
//                            printf("long press \n");
                        }
                        timer.invalidate();
                    }
                }

            }
        } else {
            printf("读取数据失败\r\n");
        }
    }

    close(fd);
}

