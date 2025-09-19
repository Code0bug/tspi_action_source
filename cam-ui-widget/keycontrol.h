#ifndef KEYCONTROL_H
#define KEYCONTROL_H

#include <QObject>
#include <QWidget>
#include <QElapsedTimer>
#include <QDateTime>
#include <QDebug>

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
#include <linux/input.h>

class KeyControl : public QObject
{
    Q_OBJECT
public:
    KeyControl();
    void key_query();

private:
    int fd;
    uint8_t cam_video_status;
    struct input_event inputevent;
    QElapsedTimer timer;

signals:
    void keyToTakephoto();
    void keyToStartvideo();
    void keyToStopvideo();
    void keyToSwitchUI();
};

#endif // KEYCONTROL_H
