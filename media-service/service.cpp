#include "service.h"

#include "media_manager.h"

#include <media_service.h>

#include <cstdio>
#include <chrono>

Service::Service(MediaManager &media)
    : media_(media)
{
}

Service::~Service()
{
    stop();
}

bool Service::start()
{
    DBusError err;
    DBusObjectPathVTable vtable = {};

    if (conn_ != nullptr)
        return false;

    dbus_threads_init_default();
    dbus_error_init(&err);

    conn_ = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (conn_ == nullptr) {
        std::fprintf(stderr, "Service: connect system bus: %s\n",
                     dbus_error_is_set(&err) ? err.message : "unknown");
        dbus_error_free(&err);
        return false;
    }
    
    dbus_connection_set_exit_on_disconnect(conn_, FALSE);

    int ret = dbus_bus_request_name(conn_, MEDIA_SERVICE_BUS_NAME,
                                    DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);
    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        std::fprintf(stderr, "Service: request name %s: %s\n",
                     MEDIA_SERVICE_BUS_NAME,
                     dbus_error_is_set(&err) ? err.message : "not primary owner");
        dbus_error_free(&err);
        goto FAIL;
    }

    vtable.message_function = &Service::onMessage;
    if (!dbus_connection_register_object_path(conn_, MEDIA_SERVICE_OBJECT_PATH,
                                              &vtable, this)) {
        std::fprintf(stderr, "Service: register object path failed\n");
        dbus_bus_release_name(conn_, MEDIA_SERVICE_BUS_NAME, nullptr);
        goto FAIL;
    }

    stopRequest_ = std::promise<void>();
    thread_ = std::thread(&Service::loop, this, stopRequest_.get_future());
    return true;

FAIL:
    dbus_connection_unref(conn_);
    conn_ = nullptr;
    return false;
}

void Service::stop()
{
    if (conn_ == nullptr)
        return;
    stopRequest_.set_value();
    if (thread_.joinable())
        thread_.join();
    dbus_connection_unregister_object_path(conn_, MEDIA_SERVICE_OBJECT_PATH);
    dbus_bus_release_name(conn_, MEDIA_SERVICE_BUS_NAME, nullptr);
    dbus_connection_unref(conn_);
    conn_ = nullptr;
}

void Service::emit(const char *signal, const char *arg)
{
    if (conn_ == nullptr)
        return;

    DBusMessage *msg = dbus_message_new_signal(
        MEDIA_SERVICE_OBJECT_PATH,
        MEDIA_SERVICE_INTERFACE,
        signal);
    if (msg == nullptr)
        return;

    if (arg != nullptr) {
        dbus_message_append_args(msg,
                                 DBUS_TYPE_STRING, &arg,
                                 DBUS_TYPE_INVALID);
    }

    dbus_connection_send(conn_, msg, nullptr);
    dbus_connection_flush(conn_);
    dbus_message_unref(msg);
}

void Service::loop(std::future<void> stopRequested)
{
    while (stopRequested.wait_for(std::chrono::milliseconds(0)) !=
           std::future_status::ready) {
        if (!dbus_connection_read_write_dispatch(conn_, 20))
            break;
    }
}

DBusHandlerResult Service::onMessage(DBusConnection *,
                                     DBusMessage *msg, void *user)
{
    auto *self = static_cast<Service *>(user);

    if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    // Start Capture
    if (dbus_message_is_method_call(msg, MEDIA_SERVICE_INTERFACE,
                                    MEDIA_SERVICE_METHOD_START_CAPTURE)) {
        self->media_.statusUpdate(MediaStatus::Preview);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    // Stop Capture
    if (dbus_message_is_method_call(msg, MEDIA_SERVICE_INTERFACE,
                                    MEDIA_SERVICE_METHOD_STOP_CAPTURE)) {
        self->media_.statusUpdate(MediaStatus::Paused);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    // Play Video
    if (dbus_message_is_method_call(msg, MEDIA_SERVICE_INTERFACE,
                                    MEDIA_SERVICE_METHOD_PLAY_VIDEO)) {
        const char *path = nullptr;
        if (!dbus_message_get_args(msg, nullptr,
                                   DBUS_TYPE_STRING, &path,
                                   DBUS_TYPE_INVALID) ||
            path == nullptr || path[0] == '\0') {
            std::fprintf(stderr, "Service: PlayVideo: invalid path\n");
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        self->media_.playPathSet(path);
        self->media_.statusUpdate(MediaStatus::Playing);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}
