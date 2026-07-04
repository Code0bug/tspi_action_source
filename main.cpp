#include "widget.h"

#include <QApplication>
#include <QFontDatabase>
#include <QScreen>
#include <csignal>

static void shutdownHandler(int)
{
    QApplication::quit();
}

void checkAndCreateUserDataDirs()
{
    const QString basePath = "/userdata";
    QStringList subDirs = {
        "imu",
        "video",
        "picture",
        "firstFrame"
    };

    QDir baseDir(basePath);

    // 如果 /userdata 本身不存在，也可以顺便创建
    if (!baseDir.exists()) {
        if (!baseDir.mkpath(".")) {
            qWarning() << "Failed to create base directory:" << basePath;
            return;
        }
    }

    for (const QString &dirName : subDirs) {
        if (!baseDir.exists(dirName)) {
            if (baseDir.mkdir(dirName)) {
            } else {
                qWarning() << "Failed to create directory:" << basePath + "/" + dirName;
            }
        }
        // 已存在：什么都不做
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    checkAndCreateUserDataDirs();

    // 旋转由 VideoGLWidget shader 内部完成，不再用 QGraphicsView
    Widget *from = new Widget; //项目主窗口

    // 设置字体
    int my_font = QFontDatabase::addApplicationFont("/usr/lib/fonts/WenQuanZhengHei-1.ttf");
    QStringList strlist(QFontDatabase::applicationFontFamilies(my_font));
    if(strlist.count()> 0)
    {
        QFont fontthis(strlist.at(0));
        a.setFont(fontthis);
    }

    // 取屏几何，全屏显示
    QRect sg = a.primaryScreen()->geometry();
    from->setGeometry(sg);
    from->showFullScreen();

    signal(SIGINT, shutdownHandler);

    return a.exec();
}
