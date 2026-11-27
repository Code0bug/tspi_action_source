/*
 * tspi QPA 平台插件
 *
 * 参考 qt5base-5.15.2/src/plugins/platforms/linuxfb 实现
 * QT_QPA_PLATFORM=linuxfb 时走 linuxfb 插件
 * QT_QPA_FB_DRM=1 时走 QLinuxFbDrmScreen（DRM） 否则走 QLinuxFbScreen（fbdev）  
 * QLinuxFbDrmScreen.cpp 中已有 DRM 显示实现，不过只用了 primary plane
 *
 * 思路：
 *  可以自己写一个 TspiDrmScreen, 引入 overlay plane + socket 接收预览数据 dma-buf fd
 *  然后使用 overlay plane 显示预览数据，同时使用 primary plane 显示 UI
 *
 * 注意点：
 *   1. primary plane 要在 overlay plane 之上
 *   2. QLinuxFbDrmScreen 用的旧接口，可以考虑换成 atomic 接口
 */
#include <qpa/qplatformintegrationplugin.h>
#include "tspi_integration.h"

QT_BEGIN_NAMESPACE

class TspiIntegrationPlugin : public QPlatformIntegrationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformIntegrationFactoryInterface_iid FILE "tspi.json")
public:
    QPlatformIntegration *create(const QString&, const QStringList&) override;
};

QPlatformIntegration *TspiIntegrationPlugin::create(const QString &system, const QStringList &paramList)
{
    if (!system.compare(QLatin1String("tspi"), Qt::CaseInsensitive))
        return new TspiIntegration(paramList);

    return 0;
}

QT_END_NAMESPACE

#include "main.moc"
