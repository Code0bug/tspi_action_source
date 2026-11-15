#include "media_client.h"

#include <media_service.h>

#include <cstdio>

#include <dbus/dbus.h>

struct MediaClient::Impl {
    DBusConnection *conn = nullptr;
};

MediaClient::MediaClient()
    : impl_(new Impl)
{
    DBusError err;
    dbus_error_init(&err);
    dbus_threads_init_default();

    impl_->conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (impl_->conn == nullptr) {
        std::fprintf(stderr, "MediaClient: connect system bus: %s\n",
                     dbus_error_is_set(&err) ? err.message : "unknown");
        dbus_error_free(&err);
        return;
    }

    dbus_connection_set_exit_on_disconnect(impl_->conn, FALSE);
}

MediaClient::~MediaClient()
{
    if (impl_->conn != nullptr)
        dbus_connection_unref(impl_->conn);
}

bool MediaClient::startCapture()
{
    return call(MEDIA_SERVICE_METHOD_START_CAPTURE);
}

bool MediaClient::stopCapture()
{
    return call(MEDIA_SERVICE_METHOD_STOP_CAPTURE);
}

bool MediaClient::call(const char *method)
{
    if (impl_->conn == nullptr)
        return false;

    DBusMessage *msg = dbus_message_new_method_call(
        MEDIA_SERVICE_BUS_NAME,
        MEDIA_SERVICE_OBJECT_PATH,
        MEDIA_SERVICE_INTERFACE,
        method);
    if (msg == nullptr) {
        std::fprintf(stderr, "MediaClient: %s: alloc message failed\n", method);
        return false;
    }

    dbus_bool_t ok = dbus_connection_send(impl_->conn, msg, nullptr);
    dbus_connection_flush(impl_->conn);
    dbus_message_unref(msg);
    if (!ok) {
        std::fprintf(stderr, "MediaClient: %s: send failed\n", method);
        return false;
    }
    return true;
}
