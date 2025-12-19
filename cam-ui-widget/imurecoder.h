#ifndef IMURECODER_H
#define IMURECODER_H

#include <QObject>
#include <QWidget>
#include <QFile>
#include <QDebug>

class IMURecoder : public QObject
{
    Q_OBJECT
public:
    explicit IMURecoder(QObject *parent = nullptr);


private:
    QFile file;
    QString timestamp;
    int recordCount;

public slots:
    void imu_record_init(QString path);
    void stop_imu_record_txt();
    void record_imu_data(std::array<signed short, 6> data);

signals:
    void IMUinitToWidget();

};

#endif // IMURECODER_H
