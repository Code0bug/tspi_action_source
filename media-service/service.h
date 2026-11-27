#ifndef MEDIA_SERVICE_SERVICE_H
#define MEDIA_SERVICE_SERVICE_H

#include <future>
#include <thread>

#include <dbus/dbus.h>

class MediaManager;

// 在 system bus 上导出媒体服务接口，接口见 ipc/include/media_service.h。
class Service {
public:
    explicit Service(MediaManager &media);
    ~Service();

    Service(const Service &) = delete;
    Service &operator=(const Service &) = delete;

    bool start();
    void stop();
    void emit(const char *signal, const char *arg = nullptr);

private:
    static DBusHandlerResult onMessage(DBusConnection *conn, DBusMessage *msg,
                                       void *user);
    void loop(std::future<void> stopRequested);

    MediaManager &media_;
    DBusConnection *conn_ = nullptr;
    std::thread thread_;
    std::promise<void> stopRequest_;
};

#endif
