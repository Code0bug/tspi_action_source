#ifndef MEDIA_CLIENT_H
#define MEDIA_CLIENT_H

#include <QObject>
#include <QString>
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
    bool playVideo(const QString &path);
    bool stopVideo();

signals:
    void recordStarted();
    void recordStopped();
    void photoStarted();
    void photoSaved(const QString &path);
    void videoSaved(const QString &path);

private slots:
    void dispatch();

private:
    bool call(const char *method, const char *arg = nullptr);
    void handleSignal(const char *member);
    static DBusHandlerResult onSignal(DBusConnection *conn, DBusMessage *msg,
                                      void *user);

    struct Impl;
    std::unique_ptr<Impl> impl_;
    QSocketNotifier *notifier_ = nullptr;
};

#endif
