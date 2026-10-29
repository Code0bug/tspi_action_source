#ifndef TSPI_DRMSCREEN_H
#define TSPI_DRMSCREEN_H

#include <QtFbSupport/private/qfbscreen_p.h>

QT_BEGIN_NAMESPACE

class TspiDrmScreen : public QFbScreen
{
    Q_OBJECT
public:
    explicit TspiDrmScreen(const QStringList &args);
    ~TspiDrmScreen() override;

    bool initialize() override;
    QRegion doRedraw() override;
    QPixmap grabWindow(WId wid, int x, int y, int width, int height) const override;
};

QT_END_NAMESPACE

#endif // TSPI_DRMSCREEN_H
