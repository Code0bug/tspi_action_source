#include "imurecoder.h"

IMURecoder::IMURecoder(QObject *parent) : QObject(parent)
{

}

void IMURecoder::imu_record_init(QString path)
{
    recordCount = 0;
    file.setFileName("/userdata/imu/" + path + ".txt");
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        qDebug() << "Unable to open file for writing:" << file.errorString();
        return;
    }
    emit IMUinitToWidget();

}

void IMURecoder::stop_imu_record_txt()
{
    file.close();
}

void IMURecoder::record_imu_data(std::array<signed short, 6> data)
{
////    auto start = std::chrono::high_resolution_clock::now();

//    QString str = QString::number(recordCount) + ","  // 时间戳
//                  + QString::number(data[0]) + ","
//                  + QString::number(data[1]) + ","
//                  + QString::number(data[2]) + ","
//                  + QString::number(data[3]) + ","
//                  + QString::number(data[4]) + ","
//                  + QString::number(data[5]) + "\n";

//    QTextStream out(&file);
//    out << str;

////    auto end = std::chrono::high_resolution_clock::now();
////    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
////    qDebug() << "record_imu_data took:" << duration.count() << "µs";

//    recordCount++;

    char buf[128];
    int len = snprintf(buf, sizeof(buf),
        "%d,%d,%d,%d,%d,%d,%d\n",
        recordCount,
        data[0], data[1], data[2], data[3], data[4], data[5]);

    file.write(buf, len);
    recordCount++;
}
