#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QWidget>
#include <unistd.h>

static void setupButton(QPushButton *btn, const QString &iconPath)
{
    btn->setIcon(QIcon(QPixmap(iconPath)));
    btn->setFixedSize(80, 80);
    btn->setIconSize(QSize(80, 80));
    btn->setStyleSheet(QStringLiteral(
        "QPushButton { border: none; background: transparent; }"
        "QPushButton:pressed { background: transparent; }"
        "QPushButton:focus { outline: none; }"));
}

static void setupOsdLabel(QLabel *lbl, int x, int y, int w, int h, Qt::Alignment align)
{
    QFont font = lbl->font();
    font.setPixelSize(25);
    font.setBold(true);
    lbl->setFont(font);
    lbl->setAlignment(align);
    lbl->setGeometry(x, y, w, h);
    lbl->setFixedSize(w, h);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel { color: white; background: transparent; padding: 2px 4px; }"));
}

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

    QWidget window;
    window.setAttribute(Qt::WA_TranslucentBackground);
    window.setStyleSheet(QStringLiteral("background: transparent;"));

    auto *timeLabel = new QLabel(&window);
    setupOsdLabel(timeLabel, 20, 20, 130, 30, Qt::AlignLeft | Qt::AlignVCenter);
    auto updateTime = [timeLabel]() {
        timeLabel->setText(QDateTime::currentDateTime().toString(QStringLiteral("HH'h'mm'm'")));
    };
    updateTime();
    auto *timeTimer = new QTimer(&window);
    QObject::connect(timeTimer, &QTimer::timeout, &window, updateTime);
    timeTimer->start(1000);

    auto *photo = new QPushButton(&window);
    setupButton(photo, QStringLiteral("/root/photo1.png"));
    photo->move(0, 100);

    auto *video = new QPushButton(&window);
    setupButton(video, QStringLiteral("/root/video1.png"));
    video->move(0, 270);

    window.showFullScreen();
    return app.exec();
}
