#ifndef MEDIA_SERVICE_IMU_MANAGER_H
#define MEDIA_SERVICE_IMU_MANAGER_H

#include "safe_queue.h"

#include <cstdio>
#include <string>
#include <thread>

#define AFS_2G  0x03
#define AFS_4G  0x02
#define AFS_8G  0x01
#define AFS_16G 0x00  // default

#define GFS_2000DPS   0x00 // default
#define GFS_1000DPS   0x01
#define GFS_500DPS    0x02
#define GFS_250DPS    0x03
#define GFS_125DPS    0x04
#define GFS_62_5DPS   0x05
#define GFS_31_25DPS  0x06
#define GFS_15_125DPS 0x07

struct ImuData {
    signed short ax, ay, az;
    signed short gx, gy, gz;
};

class ImuManager {
public:
    ImuManager();
    ~ImuManager();

    ImuManager(const ImuManager &) = delete;
    ImuManager &operator=(const ImuManager &) = delete;

    void start();
    void stop();
    void startRecord(const std::string &path);
    void stopRecord();

private:
    void record_imu_data();
    void stop_imu_record_txt();
    float bsp_Icm42688GetAres(unsigned char Ascale);
    float bsp_Icm42688GetGres(unsigned char Gscale);

    // 非静态成员函数，用于实际的数据处理
    void handleSigio(int signum);

    // 静态成员
    static ImuManager* m_instance; // 静态指针，指向当前实例
    static void sigio_signal_handler(int signum); // 静态信号处理函数（桥梁）

    int fd = -1;
    int flags;
    float accSensitivity, gyroSensitivity;

    FILE *file = nullptr;
    bool _recording;

    SafeQueue<ImuData> imuQueue;
    std::thread record_thread_;
};

#endif
