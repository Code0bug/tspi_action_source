#include "camera_window.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <unistd.h>

int main(int argc, char **argv)
{
    char executable[4096];
    ssize_t n = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    if (n > 0) {
        executable[n] = '\0';
        QDir bin = QFileInfo(QString::fromLocal8Bit(executable)).absoluteDir();
        QCoreApplication::addLibraryPath(bin.absoluteFilePath(QStringLiteral("../plugins")));
        QCoreApplication::addLibraryPath(bin.absoluteFilePath(QStringLiteral(TSPI_PLUGIN_RELATIVE)));
    }

    QApplication app(argc, argv);

    int my_font = QFontDatabase::addApplicationFont("/usr/lib/fonts/WenQuanZhengHei-1.ttf");
    QStringList strlist(QFontDatabase::applicationFontFamilies(my_font));
    if (strlist.count() > 0) {
        QFont fontthis(strlist.at(0));
        app.setFont(fontthis);
    }

    CameraWindow window;
    window.showFullScreen();
    return app.exec();
}
