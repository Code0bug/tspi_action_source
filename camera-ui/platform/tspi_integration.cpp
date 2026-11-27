#include "tspi_integration.h"
#include "tspi_drmscreen.h"

#include <QtFontDatabaseSupport/private/qgenericunixfontdatabase_p.h>
#include <QtServiceSupport/private/qgenericunixservices_p.h>
#include <QtEventDispatcherSupport/private/qgenericunixeventdispatcher_p.h>

#include <QtFbSupport/private/qfbvthandler_p.h>
#include <QtFbSupport/private/qfbbackingstore_p.h>
#include <QtFbSupport/private/qfbwindow_p.h>
#include <QtFbSupport/private/qfbcursor_p.h>

#include <QtGui/private/qguiapplication_p.h>
#include <qpa/qplatforminputcontextfactory_p.h>
#include <qpa/qwindowsysteminterface.h>

#if QT_CONFIG(evdev)
#include <QtInputSupport/private/qevdevmousemanager_p.h>
#include <QtInputSupport/private/qevdevkeyboardmanager_p.h>
#include <QtInputSupport/private/qevdevtouchmanager_p.h>
#endif

#include <QtPlatformHeaders/qlinuxfbfunctions.h>

QT_BEGIN_NAMESPACE

TspiIntegration::TspiIntegration(const QStringList &paramList)
    : m_primaryScreen(nullptr),
      m_fontDb(new QGenericUnixFontDatabase),
      m_services(new QGenericUnixServices),
      m_kbdMgr(0)
{
    m_primaryScreen = new TspiDrmScreen(paramList);
}

TspiIntegration::~TspiIntegration()
{
    QWindowSystemInterface::handleScreenRemoved(m_primaryScreen);
}

void TspiIntegration::initialize()
{
    if (m_primaryScreen->initialize())
        QWindowSystemInterface::handleScreenAdded(m_primaryScreen);
    else
        qWarning("linuxfb: Failed to initialize screen");

    m_inputContext = QPlatformInputContextFactory::create();

    m_vtHandler.reset(new QFbVtHandler);

    if (!qEnvironmentVariableIntValue("QT_QPA_FB_DISABLE_INPUT"))
        createInputHandlers();
}

bool TspiIntegration::hasCapability(QPlatformIntegration::Capability cap) const
{
    switch (cap) {
    case ThreadedPixmaps: return true;
    case WindowManagement: return false;
    default: return QPlatformIntegration::hasCapability(cap);
    }
}

QPlatformBackingStore *TspiIntegration::createPlatformBackingStore(QWindow *window) const
{
    return new QFbBackingStore(window);
}

QPlatformWindow *TspiIntegration::createPlatformWindow(QWindow *window) const
{
    return new QFbWindow(window);
}

QAbstractEventDispatcher *TspiIntegration::createEventDispatcher() const
{
    return createUnixEventDispatcher();
}

QList<QPlatformScreen *> TspiIntegration::screens() const
{
    QList<QPlatformScreen *> list;
    list.append(m_primaryScreen);
    return list;
}

QPlatformFontDatabase *TspiIntegration::fontDatabase() const
{
    return m_fontDb.data();
}

QPlatformServices *TspiIntegration::services() const
{
    return m_services.data();
}

void TspiIntegration::createInputHandlers()
{
    // 画面在 TspiDrmScreen::doRedraw 里按竖屏 DRM 逆时针 90° 画成横屏 UI
    // libinput 只把 ABS_X/ABS_Y 线性映射到 QScreen 宽高，没有 rotate，点下去会对不齐
    // 走 evdev，并用 rotate=90 让归一化坐标转同样的 90°（与截图 transpose=1 一致）
#if QT_CONFIG(evdev)
    m_kbdMgr = new QEvdevKeyboardManager(QLatin1String("EvdevKeyboard"), QString(), this);
    new QEvdevMouseManager(QLatin1String("EvdevMouse"), QString(), this);
    new QEvdevTouchManager(QLatin1String("EvdevTouch"),
                           QStringLiteral("rotate=90"), this);
#else
    Q_UNUSED(this);
#endif
}

QPlatformNativeInterface *TspiIntegration::nativeInterface() const
{
    return const_cast<TspiIntegration *>(this);
}

QFunctionPointer TspiIntegration::platformFunction(const QByteArray &function) const
{
#if QT_CONFIG(evdev)
    if (function == QLinuxFbFunctions::loadKeymapTypeIdentifier())
        return QFunctionPointer(loadKeymapStatic);
    else if (function == QLinuxFbFunctions::switchLangTypeIdentifier())
        return QFunctionPointer(switchLangStatic);
#else
    Q_UNUSED(function)
#endif

    return 0;
}

void TspiIntegration::loadKeymapStatic(const QString &filename)
{
#if QT_CONFIG(evdev)
    TspiIntegration *self = static_cast<TspiIntegration *>(QGuiApplicationPrivate::platformIntegration());
    if (self->m_kbdMgr)
        self->m_kbdMgr->loadKeymap(filename);
    else
        qWarning("TspiIntegration: Cannot load keymap, no keyboard handler found");
#else
    Q_UNUSED(filename);
#endif
}

void TspiIntegration::switchLangStatic()
{
#if QT_CONFIG(evdev)
    TspiIntegration *self = static_cast<TspiIntegration *>(QGuiApplicationPrivate::platformIntegration());
    if (self->m_kbdMgr)
        self->m_kbdMgr->switchLang();
    else
        qWarning("TspiIntegration: Cannot switch language, no keyboard handler found");
#endif
}

QT_END_NAMESPACE
