#ifndef IMUREADER_H
#define IMUREADER_H

#include <QObject>
#include <QWidget>
#include <QTimer>
#include <QFile>
#include <QDebug>
#include <QTextStream>

#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "sys/ioctl.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include <poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>

#include <vector>

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


class IMUReader : public QObject
{
    Q_OBJECT
public:
    explicit IMUReader(QObject *parent = nullptr);
    void imu_get_data();

private:
    float bsp_Icm42688GetAres(unsigned char Ascale);
    float bsp_Icm42688GetGres(unsigned char Gscale);
    signed short ax, ay, az, gx, gy, gz, tem;
    int fd;
    float accSensitivity, gyroSensitivity;

public slots:

signals:
    void IMUreadTorecord(std::array<signed short, 6> data);


};

#endif // IMUREADER_H
