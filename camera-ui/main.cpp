#include <QApplication>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include <QThread>
#include <QSocketNotifier>
#include <QWidget>
#include <sys/signalfd.h>
#include <signal.h>
#include <unistd.h>

class PreviewWindow : public QWidget {
public:
    explicit PreviewWindow(const QImage &image) : image_(image) {
        setAttribute(Qt::WA_TranslucentBackground);
        setWindowFlags(Qt::FramelessWindowHint);
        auto *timer = new QTimer(this);
        QObject::connect(timer, &QTimer::timeout, this, [this] { ++seconds_; update(0, 0, width(), 52); });
        timer->start(1000);
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.fillRect(0, 0, width(), 52, QColor(0, 0, 0, 150));
        painter.setPen(Qt::white);
        painter.drawText(QRect(20, 0, width() - 40, 52), Qt::AlignVCenter,
                         QStringLiteral("Camera preview    UI %1 s").arg(seconds_));
        if (!image_.isNull()) {
            QRect target(QPoint(), image_.size());
            target.moveCenter(rect().center());
            painter.drawImage(target, image_);
        }
    }
private:
    QImage image_;
    unsigned seconds_ = 0;
};

int main(int argc, char **argv)
{
    // Set this before QApplication starts the display and input threads.
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT); sigaddset(&mask, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &mask, nullptr)) return 1;
    int signalFd = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
    if (signalFd < 0) return 1;
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "tspi");
    qputenv("QT_QPA_FB_HIDECURSOR", "1");
    // Support both the build tree and a relocatable staged installation.
    char executable[4096];
    ssize_t count = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    if (count > 0) {
        executable[count] = '\0';
        QDir bin = QFileInfo(QString::fromLocal8Bit(executable)).absoluteDir();
        QCoreApplication::addLibraryPath(bin.absoluteFilePath(QStringLiteral("../plugins")));
        QCoreApplication::addLibraryPath(bin.absoluteFilePath(QStringLiteral(TSPI_PLUGIN_RELATIVE)));
    }
    QApplication application(argc, argv);
    QSocketNotifier notifier(signalFd, QSocketNotifier::Read);
    QObject::connect(&notifier, &QSocketNotifier::activated, &application, [&application, signalFd] {
        signalfd_siginfo event{};
        while (read(signalFd, &event, sizeof(event)) == sizeof(event)) {}
        application.quit();
    });
    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption stall(QStringLiteral("test-gui-stall-ms"),
        QStringLiteral("Block the GUI thread once after 5 seconds to check independent video."), QStringLiteral("ms"));
    parser.addOption(stall);
    QCommandLineOption testImage(QStringLiteral("test-image"),
        QStringLiteral("Draw an image at the center of the transparent UI layer."), QStringLiteral("path"));
    parser.addOption(testImage);
    parser.process(application);
    QImage image;
    if (parser.isSet(testImage) && !image.load(parser.value(testImage)))
        qFatal("camera-ui: cannot load test image %s", qPrintable(parser.value(testImage)));
    if (parser.isSet(stall)) {
        bool valid = false;
        unsigned milliseconds = parser.value(stall).toUInt(&valid);
        if (!valid || !milliseconds || milliseconds > 10000) qFatal("test-gui-stall-ms must be 1..10000");
        QTimer::singleShot(5000, &application, [milliseconds] {
            qInfo("camera-ui: GUI stall test begins");
            QThread::msleep(milliseconds);
            qInfo("camera-ui: GUI stall test ends");
        });
    }
    PreviewWindow window(image);
    window.showFullScreen();
    int result = application.exec();
    notifier.setEnabled(false);
    close(signalFd);
    return result;
}
