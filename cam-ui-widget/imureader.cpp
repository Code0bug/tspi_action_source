#include "imureader.h"

int interval_ms = 5; // 200 Hz 采样

IMUReader::IMUReader(QObject *parent) : QObject(parent)
{
    // 打开并初始化imu
    fd = open("/dev/icm42688p", O_RDWR);
    if (fd < 0) {
        printf("Can't open icm42688p\r\n");
    }

    accSensitivity   = 0.244f;
    gyroSensitivity  = 32.8f;

    bsp_Icm42688GetAres(AFS_4G);
    bsp_Icm42688GetGres(GFS_1000DPS);
}



void IMUReader::imu_get_data()
{
    signed short databuf[7];

    int ret = read(fd, databuf, sizeof(databuf));
    if (ret == 0) {
        // 读取原始数据
//        ax = databuf[0];
//        ay = databuf[1];
//        az = databuf[2];
//        gx = databuf[3];
//        gy = databuf[4];
//        gz = databuf[5];

        emit IMUreadTorecord({databuf[0], databuf[1], databuf[2], databuf[3], databuf[4], databuf[5]});

//        tem = databuf[6] / 132.48 + 25;

//        printf("ax: %d, ay: %d, az: %d, gx: %d, gy: %d, gz: %d, tem: %d\r\n", ax, ay, az, gx, gy, gz, tem);
    }

}

float IMUReader::bsp_Icm42688GetAres(unsigned char Ascale)
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

float IMUReader::bsp_Icm42688GetGres(unsigned char Gscale)
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
