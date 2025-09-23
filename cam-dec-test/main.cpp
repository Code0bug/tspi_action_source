#include "widget.h"

#include <QApplication>
#include <QGraphicsView>
#include <QGraphicsProxyWidget>
#include <QFontDatabase>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);


    // 旋转屏幕
    Widget *from = new Widget; //项目主窗口
    QGraphicsScene *scene = new QGraphicsScene;
    QGraphicsProxyWidget *w = scene->addWidget(from);
    w->setRotation(90);
    QGraphicsView *view = new QGraphicsView(scene);

    // 设置字体
    int my_font = QFontDatabase::addApplicationFont("/usr/lib/fonts/WenQuanZhengHei-1.ttf");
    QStringList strlist(QFontDatabase::applicationFontFamilies(my_font));
    if(strlist.count()> 0)
    {
        QFont fontthis(strlist.at(0));
        a.setFont(fontthis);
    }


    view->show();

    return a.exec();
}
