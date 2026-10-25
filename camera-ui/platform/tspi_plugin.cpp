#include "display_thread.h"

#include <qpa/qplatformintegration.h>
#include <qpa/qplatformintegrationplugin.h>
#include <qpa/qplatforminputcontextfactory_p.h>
#include <qpa/qplatforminputcontext.h>
#include <qpa/qwindowsysteminterface.h>
#include <QtFbSupport/private/qfbscreen_p.h>
#include <QtFbSupport/private/qfbwindow_p.h>
#include <QtFbSupport/private/qfbbackingstore_p.h>
#include <QtFontDatabaseSupport/private/qgenericunixfontdatabase_p.h>
#include <QtEventDispatcherSupport/private/qgenericunixeventdispatcher_p.h>
#include <QtServiceSupport/private/qgenericunixservices_p.h>
#include <QtInputSupport/private/qevdevtouchmanager_p.h>
#include <QtInputSupport/private/qevdevkeyboardmanager_p.h>
#include <QtInputSupport/private/qevdevmousemanager_p.h>
#include <QScopedPointer>
#include <QDebug>

class TspiScreen : public QFbScreen {
public:
    bool initialize() override {
        QSize physical = display_.start();
        if (physical.isEmpty()) return false;
        mGeometry = QRect(QPoint(), physical.transposed());
        mDepth = 32;
        mFormat = QImage::Format_ARGB32_Premultiplied;
        mPhysicalSize = QSizeF(mGeometry.width() * 25.4 / 96, mGeometry.height() * 25.4 / 96);
        initializeCompositor();
        mScreenImage.fill(Qt::transparent);
        return true;
    }
protected:
    QRegion doRedraw() override {
        QRegion changed = QFbScreen::doRedraw();
        if (!changed.isEmpty()) display_.publish(mScreenImage, changed);
        return changed;
    }
private:
    DisplayThread display_;
};

class TspiIntegration : public QObject, public QPlatformIntegration {
public:
    TspiIntegration() : screen_(new TspiScreen), fonts_(new QGenericUnixFontDatabase),
                        services_(new QGenericUnixServices) {}
    ~TspiIntegration() override {
        /* Stop input before removing the screen and joining its display thread. */
        touch_.reset(); mouse_.reset(); keyboard_.reset();
        inputContext_.reset();
        QWindowSystemInterface::handleScreenRemoved(screen_);
    }
    void initialize() override {
        if (!screen_->initialize()) qFatal("tspi: failed to initialize atomic DRM display");
        QWindowSystemInterface::handleScreenAdded(screen_);
        inputContext_.reset(QPlatformInputContextFactory::create());
        if (!qEnvironmentVariableIntValue("TSPI_DISABLE_INPUT")) {
            /* Physical -> logical is the inverse of clockwise scanout rotation.
             * This Qt build reads the environment instead of the constructor spec.
             * Preserve user device/inversion options while enforcing one rotation. */
            QStringList spec = QString::fromLocal8Bit(qgetenv("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS"))
                                   .split(QLatin1Char(':'), Qt::SkipEmptyParts);
            for (int i = spec.size() - 1; i >= 0; --i)
                if (spec[i].startsWith(QLatin1String("rotate="))) spec.removeAt(i);
            spec.append(QStringLiteral("rotate=270"));
            qputenv("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS", spec.join(QLatin1Char(':')).toLocal8Bit());
            keyboard_.reset(new QEvdevKeyboardManager(QStringLiteral("EvdevKeyboard"), QString(), this));
            mouse_.reset(new QEvdevMouseManager(QStringLiteral("EvdevMouse"), QString(), this));
            touch_.reset(new QEvdevTouchManager(QStringLiteral("EvdevTouch"), QString(), this));
        }
    }
    bool hasCapability(Capability cap) const override {
        if (cap == ThreadedPixmaps) return true;
        if (cap == WindowManagement || cap == OpenGL) return false;
        return QPlatformIntegration::hasCapability(cap);
    }
    QPlatformWindow *createPlatformWindow(QWindow *window) const override { return new QFbWindow(window); }
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const override { return new QFbBackingStore(window); }
    QAbstractEventDispatcher *createEventDispatcher() const override { return createUnixEventDispatcher(); }
    QPlatformFontDatabase *fontDatabase() const override { return fonts_.data(); }
    QPlatformServices *services() const override { return services_.data(); }
    QPlatformInputContext *inputContext() const override { return inputContext_.data(); }
private:
    TspiScreen *screen_;
    QScopedPointer<QGenericUnixFontDatabase> fonts_;
    QScopedPointer<QGenericUnixServices> services_;
    QScopedPointer<QPlatformInputContext> inputContext_;
    QScopedPointer<QEvdevTouchManager> touch_;
    QScopedPointer<QEvdevKeyboardManager> keyboard_;
    QScopedPointer<QEvdevMouseManager> mouse_;
};

class TspiPlugin : public QPlatformIntegrationPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformIntegrationFactoryInterface_iid FILE "tspi.json")
public:
    QPlatformIntegration *create(const QString &key, const QStringList &) override {
        return key.compare(QLatin1String("tspi"), Qt::CaseInsensitive) == 0 ? new TspiIntegration : nullptr;
    }
};

#include "tspi_plugin.moc"
