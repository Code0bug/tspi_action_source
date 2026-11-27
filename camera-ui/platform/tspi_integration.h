#ifndef TSPI_INTEGRATION_H
#define TSPI_INTEGRATION_H

#include <qpa/qplatformintegration.h>
#include <qpa/qplatformnativeinterface.h>

QT_BEGIN_NAMESPACE

class QAbstractEventDispatcher;
class QFbScreen;
class QFbVtHandler;
class QEvdevKeyboardManager;

class TspiIntegration : public QPlatformIntegration, public QPlatformNativeInterface
{
public:
    TspiIntegration(const QStringList &paramList);
    ~TspiIntegration();

    void initialize() override;
    bool hasCapability(QPlatformIntegration::Capability cap) const override;

    QPlatformWindow *createPlatformWindow(QWindow *window) const override;
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const override;

    QAbstractEventDispatcher *createEventDispatcher() const override;

    QPlatformFontDatabase *fontDatabase() const override;
    QPlatformServices *services() const override;
    QPlatformInputContext *inputContext() const override { return m_inputContext; }

    QPlatformNativeInterface *nativeInterface() const override;

    QList<QPlatformScreen *> screens() const;

    QFunctionPointer platformFunction(const QByteArray &function) const override;

private:
    void createInputHandlers();
    static void loadKeymapStatic(const QString &filename);
    static void switchLangStatic();

    QFbScreen *m_primaryScreen;
    QPlatformInputContext *m_inputContext;
    QScopedPointer<QPlatformFontDatabase> m_fontDb;
    QScopedPointer<QPlatformServices> m_services;
    QScopedPointer<QFbVtHandler> m_vtHandler;

    QEvdevKeyboardManager *m_kbdMgr;
};

QT_END_NAMESPACE

#endif // TSPI_INTEGRATION_H
