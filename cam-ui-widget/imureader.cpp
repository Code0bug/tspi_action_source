#include "imureader.h"

IMUReader* IMUReader::m_instance = nullptr;

IMUReader::IMUReader(QObject *parent) : QObject(parent)
{
    m_instance = this;

    // 打开并初始化imu
    fd = open("/dev/icm42688p", O_RDWR);
    if (fd < 0) {
        printf("Can't open icm42688p\r\n");
    }

    accSensitivity   = 0.244f;
    gyroSensitivity  = 32.8f;

    bsp_Icm42688GetAres(AFS_4G);
    bsp_Icm42688GetGres(GFS_1000DPS);

    recordCount = 0;
    _recording = 0;

    // 设置信号SIGIO的处理函数
    signal(SIGIO, IMUReader::sigio_signal_handler);

    fcntl(fd, F_SETOWN, getpid());		// 设置当前进程接收SIGIO信号
    flags = fcntl(fd, F_GETFL);			// 获取当前的进程状态
    fcntl(fd, F_SETFL, flags | FASYNC);	// 设置进程启用异步通知功能

}

// 实现静态信号处理函数（桥梁）
void IMUReader::sigio_signal_handler(int signum)
{
    // 通过静态指针 m_instance 调用非静态成员函数 handleSigio
    if (m_instance) {
        m_instance->handleSigio(signum);
    }
}


// 实际信号处理函数
void IMUReader::handleSigio(int signum) {

    int ret = 0;

    signed short databuf[7];
//    signed short ax, ay, az, gx, gy, gz;
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

//            printf("ax: %d, ay: %d, az: %d, gx: %d, gy: %d, gz: %d\r\n", m_data.ax, m_data.ay, m_data.az, m_data.gx, m_data.gy, m_data.gz);
        }

//        tem = databuf[6] / 132.48 + 25;


//         printf("ax: %d, ay: %d, az: %d, gx: %d, gy: %d, gz: %d\r\n", ax, ay, az, gx, gy, gz);

    }

//    printf("sigio_signal_func\n");
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

    // 强制刷盘
    file.flush();                       // Qt 缓冲 → 内核缓存
    int fd = file.handle();             // 获取底层文件描述符
    if (fd >= 0) {
        fsync(fd);                       // 内核缓存 → eMMC
    }

    file.close();

    recordCount = 0;
    _recording = 0;
}

void IMUReader::record_imu_data(QString path)
{

    _recording = 1;  // 开始记录

    if (recordCount == 0) {  // 首次进入创建txt文件
        file.setFileName("/userdata/imu/" + path + ".txt");
        if (!file.open(QIODevice::Append | QIODevice::Text)) {
            qDebug() << "Unable to open file for writing:" << file.errorString();
            return;
        }
    }

    ImuData m_data;

    while (_recording) {
        if (imuQueue.pop(m_data, 10)) {  // 取数据并保存

//            auto start = std::chrono::high_resolution_clock::now();


            QString str = QString::number(recordCount) + ","  // 时间戳
                          + QString::number(m_data.gx) + ","
                          + QString::number(m_data.gy) + ","
                          + QString::number(m_data.gz) + ","
                          + QString::number(m_data.ax) + ","
                          + QString::number(m_data.ay) + ","
                          + QString::number(m_data.az) + "\n";

            QTextStream out(&file);
            out << str;

            recordCount++;

//            auto end = std::chrono::high_resolution_clock::now();
//            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
//            qDebug() << "record_imu_data took:" << duration.count() << "µs";

        }
    }










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
