#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QWidget>
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
    QWidget window;
    window.showFullScreen();
    return app.exec();
}
