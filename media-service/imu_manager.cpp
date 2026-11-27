#include "imu_manager.h"

#include <fcntl.h>

#ifndef FASYNC
#define FASYNC O_ASYNC
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

ImuManager* ImuManager::m_instance = nullptr;

ImuManager::ImuManager()
{
}

ImuManager::~ImuManager()
{
}

void ImuManager::start()
{
    m_instance = this;
    file = nullptr;

    // 打开并初始化imu
    fd = open("/dev/icm42688p", O_RDWR);
    if (fd < 0) {
        printf("Can't open icm42688p\r\n");
    }

    // Sensitivity 系数目前可以放到 python 后处理代码中
    // Media 程序中只记录原始数据，以加快处理速度
    // accSensitivity   = 0.244f;
    // gyroSensitivity  = 32.8f;

    // bsp_Icm42688GetAres(AFS_4G);
    // bsp_Icm42688GetGres(GFS_1000DPS);

    _recording = 0;

    // 设置信号SIGIO的处理函数
    signal(SIGIO, ImuManager::sigio_signal_handler);

    fcntl(fd, F_SETOWN, getpid());		// 设置当前进程接收SIGIO信号
    flags = fcntl(fd, F_GETFL);			// 获取当前的进程状态
    fcntl(fd, F_SETFL, flags | FASYNC);	// 设置进程启用异步通知功能
}

void ImuManager::stop()
{
    stopRecord();
    if (m_instance == this)
        m_instance = nullptr;
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}

void ImuManager::startRecord(const std::string &path)
{
    std::string filename = "/userdata/imu/" + path + ".txt";
    file = fopen(filename.c_str(), "a");
    if (!file) {
        printf("Unable to open file for writing\r\n");
        return;
    }
    record_thread_ = std::thread(&ImuManager::record_imu_data, this);
}

void ImuManager::stopRecord()
{
    if (record_thread_.joinable()) {
        stop_imu_record_txt();
        record_thread_.join();
    }
}

// 实现静态信号处理函数（桥梁）
void ImuManager::sigio_signal_handler(int signum)
{
    // 通过静态指针 m_instance 调用非静态成员函数 handleSigio
    if (m_instance) {
        m_instance->handleSigio(signum);
    }
}


// 实际信号处理函数
void ImuManager::handleSigio(int signum) {

    int ret = 0;

    signed short databuf[7];
    ImuData m_data;

    ret = read(fd, databuf, sizeof(databuf));
    if (ret == 0) {

        if (_recording) { // 开始录像时，读取数据并送入队列
            m_data.ax = databuf[0];
            m_data.ay = databuf[1];
            m_data.az = databuf[2];
            m_data.gx = databuf[3];
            m_data.gy = databuf[4];
            m_data.gz = databuf[5];

            imuQueue.push(m_data);
            // printf("ax: %d, ay: %d, az: %d, gx: %d, gy: %d, gz: %d\r\n", m_data.ax, m_data.ay, m_data.az, m_data.gx, m_data.gy, m_data.gz);
        }
    }

}

void ImuManager::stop_imu_record_txt()
{
    // 强制刷盘
    if (file) {
        fflush(file);       
        int fd = fileno(file);    // 获取底层文件描述符
        if (fd >= 0) {
            fsync(fd);                    // 内核缓存 → eMMC
        }

        fclose(file);
        file = nullptr;
    }

    _recording = 0;
}

void ImuManager::record_imu_data()
{

    _recording = 1;  // 开始记录

    ImuData m_data;
    int n = 0;

    while (_recording) {
        if (imuQueue.pop(m_data, 10)) {  // 取数据并保存
            char str[128];
            snprintf(str, sizeof(str), "%d,%d,%d,%d,%d,%d,%d\n",
                     n,  // 时间戳
                     m_data.gx,
                     m_data.gy,
                     m_data.gz,
                     m_data.ax,
                     m_data.ay,
                     m_data.az);

            fputs(str, file);
            n++;
        }
    }
}

float ImuManager::bsp_Icm42688GetAres(unsigned char Ascale)
{
    switch(Ascale)
    {
    // Possible accelerometer scales (and their register bit settings) are:
    // 2 Gs (11), 4 Gs (10), 8 Gs (01), and 16 Gs  (00).
    case AFS_2G:
        accSensitivity = 2000 / 32768.0f;
        break;
    case AFS_4G:
        accSensitivity = 4000 / 32768.0f;
        break;
    case AFS_8G:
        accSensitivity = 8000 / 32768.0f;
        break;
    case AFS_16G:
        accSensitivity = 16000 / 32768.0f;
        break;
    }

    return accSensitivity;
}

float ImuManager::bsp_Icm42688GetGres(unsigned char Gscale)
{
    switch(Gscale)
    {
    case GFS_15_125DPS:
        gyroSensitivity = 15.125f / 32768.0f;
        break;
    case GFS_31_25DPS:
        gyroSensitivity = 31.25f / 32768.0f;
        break;
    case GFS_62_5DPS:
        gyroSensitivity = 62.5f / 32768.0f;
        break;
    case GFS_125DPS:
        gyroSensitivity = 125.0f / 32768.0f;
        break;
    case GFS_250DPS:
        gyroSensitivity = 250.0f / 32768.0f;
        break;
    case GFS_500DPS:
        gyroSensitivity = 500.0f / 32768.0f;
        break;
    case GFS_1000DPS:
        gyroSensitivity = 1000.0f / 32768.0f;
        break;
    case GFS_2000DPS:
        gyroSensitivity = 2000.0f / 32768.0f;
        break;
    }
    return gyroSensitivity;
}
