#include "tspi_drmscreen.h"

QT_BEGIN_NAMESPACE

TspiDrmScreen::TspiDrmScreen(const QStringList &args)
{
    Q_UNUSED(args);
}

TspiDrmScreen::~TspiDrmScreen() = default;

// 初始化屏幕
bool TspiDrmScreen::initialize()
{
    return QFbScreen::initialize();
}

// 处理屏幕重绘
QRegion TspiDrmScreen::doRedraw()
{
    return QFbScreen::doRedraw();
}

// 获取屏幕或窗口的截图
QPixmap TspiDrmScreen::grabWindow(WId wid, int x, int y, int width, int height) const
{
    Q_UNUSED(wid);
    Q_UNUSED(x);
    Q_UNUSED(y);
    Q_UNUSED(width);
    Q_UNUSED(height);

    return QPixmap();
}

QT_END_NAMESPACE
