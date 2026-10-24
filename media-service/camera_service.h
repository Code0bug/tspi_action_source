#ifndef MEDIA_SERVICE_CAMERA_SERVICE_H
#define MEDIA_SERVICE_CAMERA_SERVICE_H

#include <atomic>
#include <thread>

#include <dbus/dbus.h>

class CameraManager;

// 在 system bus 上导出 CameraManager 的开关控制，接口见 ipc/include/camera_ipc.h。
class CameraService {
public:
    explicit CameraService(CameraManager &camera) : camera_(camera) {}
    ~CameraService();

    CameraService(const CameraService &) = delete;
    CameraService &operator=(const CameraService &) = delete;

    bool start();
    void stop();

private:
    static DBusHandlerResult onMessage(DBusConnection *conn, DBusMessage *msg,
                                       void *user);
    DBusHandlerResult handle(DBusConnection *conn, DBusMessage *msg);
    void emitStateChanged(DBusConnection *conn, bool capturing);
    void loop();

    CameraManager &camera_;
    DBusConnection *conn_ = nullptr;
    std::thread thread_;
    std::atomic<bool> running_{false};
};

#endif
