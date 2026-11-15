#ifndef MEDIA_CLIENT_H
#define MEDIA_CLIENT_H

#include <QObject>
#include <memory>

#include <dbus/dbus.h>

class QSocketNotifier;

class MediaClient : public QObject
{
    Q_OBJECT

public:
    explicit MediaClient(QObject *parent = nullptr);
    ~MediaClient();

    MediaClient(const MediaClient &) = delete;
    MediaClient &operator=(const MediaClient &) = delete;

    bool startCapture();
    bool stopCapture();

signals:
    void recordStarted();
    void recordStopped();

private slots:
    void dispatch();

private:
    bool call(const char *method);
    void handleSignal(const char *member);
    static DBusHandlerResult onSignal(DBusConnection *conn, DBusMessage *msg,
                                      void *user);

    struct Impl;
    std::unique_ptr<Impl> impl_;
    QSocketNotifier *notifier_ = nullptr;
};

#endif
