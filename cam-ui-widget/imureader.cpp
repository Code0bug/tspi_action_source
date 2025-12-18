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

void IMUReader::imu_record_init(QString path)
{
    recordCount = 0;
    file.setFileName("/userdata/imu/" + path + ".txt");
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        qDebug() << "Unable to open file for writing:" << file.errorString();
        return;
    }
    emit IMUinitToWidget();

}

void IMUReader::stop_imu_record_txt()
{
    file.close();
}

void IMUReader::record_imu_data()
{
//    auto start = std::chrono::high_resolution_clock::now();

    imu_get_data();
    QString str = QString::number(recordCount) + ","  // 时间戳
                  + QString::number(gx) + ","
                  + QString::number(gy) + ","
                  + QString::number(gz) + ","
                  + QString::number(ax) + ","
                  + QString::number(ay) + ","
                  + QString::number(az) + "\n";

    QTextStream out(&file);
    out << str;

//    auto end = std::chrono::high_resolution_clock::now();
//    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
//    qDebug() << "record_imu_data took:" << duration.count() << "µs";

    recordCount++;
}



void IMUReader::imu_get_data()
{
    signed short databuf[7];

    int ret = read(fd, databuf, sizeof(databuf));
    if (ret == 0) {
//        ax = databuf[0] * accSensitivity;
//        ay = databuf[1] * accSensitivity;
//        az = databuf[2] * accSensitivity;
//        gx = databuf[3] * gyroSensitivity;
//        gy = databuf[4] * gyroSensitivity;
//        gz = databuf[5] * gyroSensitivity;

        ax = databuf[0];
        ay = databuf[1];
        az = databuf[2];
        gx = databuf[3];
        gy = databuf[4];
        gz = databuf[5];
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
