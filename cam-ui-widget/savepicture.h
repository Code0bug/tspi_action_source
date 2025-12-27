#ifndef SAVEPICTURE_H
#define SAVEPICTURE_H

#include <iostream>
#include <fcntl.h>    // open, O_RDONLY
#include <unistd.h>   // fsync, close

#include <QObject>
#include <QWidget>
#include <QString>
#include <opencv2/opencv.hpp>

class SavePicture : public QObject
{
    Q_OBJECT
public:
    explicit SavePicture(QObject *parent = nullptr);

public slots:
    bool do_save_picture(QString savePath, cv::Mat img);

signals:
    void photoFinishedToUpdate(QString path);

};

#endif // SAVEPICTURE_H
