#include "savepicture.h"

SavePicture::SavePicture(QObject *parent) : QObject(parent)
{

}

bool SavePicture::do_save_picture(QString savePath, cv::Mat img)
{
    // 写入文件
    if (!cv::imwrite(savePath.toStdString(), img)) {
        fprintf(stderr, "imwrite failed: %s\n", savePath.toStdString().c_str());
        return false;
    }

    // 强制刷盘，保证数据写入 eMMC
    int fd = open(savePath.toStdString().c_str(), O_RDONLY);
    if (fd >= 0) {
        if (fsync(fd) != 0) {
            perror("fsync failed");
            close(fd);
            return false;
        }
        close(fd);
    } else {
        perror("open file failed");
        return false;
    }

    printf("taking photo: %s\n", savePath.toStdString().c_str());

    emit photoFinishedToUpdate(savePath);  // 每次拍完照片更新相册

    return true;
}

