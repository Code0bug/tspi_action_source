#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QLabel>
#include <QPushButton>
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

    // 添加字体
    int my_font = QFontDatabase::addApplicationFont("/usr/lib/fonts/WenQuanZhengHei-1.ttf");
    QStringList strlist(QFontDatabase::applicationFontFamilies(my_font));
    if (strlist.count() > 0) {
        QFont fontthis(strlist.at(0));
        app.setFont(fontthis);
    }

    QWidget window;
    // 添加测试 UI
    window.setAttribute(Qt::WA_TranslucentBackground);
    window.setStyleSheet(QStringLiteral("background: transparent;"));

    QLabel *label = new QLabel(QStringLiteral("tspi preview"), &window);
    label->setStyleSheet(QStringLiteral(
        "color: white; background: rgba(0, 0, 0, 160); padding: 12px; font-size: 20px;"));
    label->move(24, 24);
    label->adjustSize();

    QPushButton *button = new QPushButton(QStringLiteral("Capture"), &window);
    button->setStyleSheet(QStringLiteral(
        "background: #e53935; color: white; font-size: 18px; border: none; padding: 12px 28px;"));
    button->move(24, 80);
    button->adjustSize();

    QPushButton *mode = new QPushButton(QStringLiteral("Mode"), &window);
    mode->setStyleSheet(QStringLiteral(
        "background: #1e88e5; color: white; font-size: 18px; border: none; padding: 12px 28px;"));
    mode->move(24, 140);
    mode->adjustSize();
    QObject::connect(mode, &QPushButton::clicked, label, [label]() {
        label->setText(QStringLiteral("mode clicked"));
        label->adjustSize();
    });

    window.showFullScreen();
    return app.exec();
}
