#include "camera_service.h"

#include "camera_manager.h"
#include <camera_ipc.h>

#include <cstdio>

static const char *kIntrospectXml =
    DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE
    "<node>\n"
    "  <interface name=\"" CAMERA_IPC_INTERFACE "\">\n"
    "    <method name=\"" CAMERA_IPC_METHOD_START_CAPTURE "\">\n"
    "      <arg type=\"b\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"" CAMERA_IPC_METHOD_STOP_CAPTURE "\">\n"
    "      <arg type=\"b\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"" CAMERA_IPC_METHOD_IS_CAPTURING "\">\n"
    "      <arg type=\"b\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <signal name=\"" CAMERA_IPC_SIGNAL_STATE_CHANGED "\">\n"
    "      <arg type=\"b\"/>\n"
    "    </signal>\n"
    "  </interface>\n"
    "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
    "    <method name=\"Introspect\">\n"
    "      <arg type=\"s\" direction=\"out\"/>\n"
    "    </method>\n"
    "  </interface>\n"
    "</node>\n";

CameraService::~CameraService()
{
    stop();
}

bool CameraService::start()
{
    DBusError err;
    DBusObjectPathVTable vtable = {};

    if (running_)
        return false;

    dbus_threads_init_default();
    dbus_error_init(&err);
    conn_ = dbus_bus_get_private(DBUS_BUS_SYSTEM, &err);
    if (conn_ == nullptr) {
        std::fprintf(stderr, "CameraService: connect system bus: %s\n",
                     dbus_error_is_set(&err) ? err.message : "unknown");
        dbus_error_free(&err);
        return false;
    }
    dbus_connection_set_exit_on_disconnect(conn_, FALSE);

    int ret = dbus_bus_request_name(conn_, CAMERA_IPC_BUS_NAME,
                                    DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);
    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        std::fprintf(stderr, "CameraService: request name %s: %s\n",
                     CAMERA_IPC_BUS_NAME,
                     dbus_error_is_set(&err) ? err.message : "not primary owner");
        dbus_error_free(&err);
        goto FAIL;
    }

    vtable.message_function = &CameraService::onMessage;
    if (!dbus_connection_register_object_path(conn_, CAMERA_IPC_OBJECT_PATH,
                                              &vtable, this)) {
        std::fprintf(stderr, "CameraService: register object path failed\n");
        goto FAIL;
    }

    running_ = true;
    thread_ = std::thread(&CameraService::loop, this);
    return true;

FAIL:
    dbus_connection_close(conn_);
    dbus_connection_unref(conn_);
    conn_ = nullptr;
    return false;
}

void CameraService::stop()
{
    if (!running_.exchange(false))
        return;
    if (thread_.joinable())
        thread_.join();
    dbus_connection_close(conn_);
    dbus_connection_unref(conn_);
    conn_ = nullptr;
}

void CameraService::loop()
{
    while (running_) {
        if (!dbus_connection_read_write_dispatch(conn_, 200))
            break;
    }
}

DBusHandlerResult CameraService::onMessage(DBusConnection *conn,
                                           DBusMessage *msg, void *user)
{
    return static_cast<CameraService *>(user)->handle(conn, msg);
}

static void replyBool(DBusConnection *conn, DBusMessage *msg, bool value)
{
    DBusMessage *reply = dbus_message_new_method_return(msg);
    DBusMessageIter it;
    dbus_bool_t v = value ? TRUE : FALSE;

    if (reply == nullptr)
        return;
    dbus_message_iter_init_append(reply, &it);
    dbus_message_iter_append_basic(&it, DBUS_TYPE_BOOLEAN, &v);
    dbus_connection_send(conn, reply, nullptr);
    dbus_message_unref(reply);
}

DBusHandlerResult CameraService::handle(DBusConnection *conn, DBusMessage *msg)
{
    if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Introspectable",
                                    "Introspect")) {
        DBusMessage *reply = dbus_message_new_method_return(msg);
        if (reply != nullptr) {
            dbus_message_append_args(reply, DBUS_TYPE_STRING, &kIntrospectXml,
                                     DBUS_TYPE_INVALID);
            dbus_connection_send(conn, reply, nullptr);
            dbus_message_unref(reply);
        }
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (dbus_message_is_method_call(msg, CAMERA_IPC_INTERFACE,
                                    CAMERA_IPC_METHOD_START_CAPTURE) ||
        dbus_message_is_method_call(msg, CAMERA_IPC_INTERFACE,
                                    CAMERA_IPC_METHOD_STOP_CAPTURE)) {
        bool start = dbus_message_is_method_call(msg, CAMERA_IPC_INTERFACE,
                                                 CAMERA_IPC_METHOD_START_CAPTURE);
        bool before = camera_.isCapturing();

        if (!camera_.isOpen()) {
            DBusMessage *err = dbus_message_new_error(msg, DBUS_ERROR_FAILED,
                                                      "camera not opened");
            if (err != nullptr) {
                dbus_connection_send(conn, err, nullptr);
                dbus_message_unref(err);
            }
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        if (start)
            camera_.startCapture();
        else
            camera_.stopCapture();

        bool now = camera_.isCapturing();
        replyBool(conn, msg, now);
        if (now != before)
            emitStateChanged(conn, now);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (dbus_message_is_method_call(msg, CAMERA_IPC_INTERFACE,
                                    CAMERA_IPC_METHOD_IS_CAPTURING)) {
        replyBool(conn, msg, camera_.isCapturing());
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    DBusMessage *err = dbus_message_new_error(msg, DBUS_ERROR_UNKNOWN_METHOD,
                                              "unknown method");
    if (err != nullptr) {
        dbus_connection_send(conn, err, nullptr);
        dbus_message_unref(err);
    }
    return DBUS_HANDLER_RESULT_HANDLED;
}

void CameraService::emitStateChanged(DBusConnection *conn, bool capturing)
{
    DBusMessage *sig = dbus_message_new_signal(CAMERA_IPC_OBJECT_PATH,
                                               CAMERA_IPC_INTERFACE,
                                               CAMERA_IPC_SIGNAL_STATE_CHANGED);
    dbus_bool_t v = capturing ? TRUE : FALSE;

    if (sig == nullptr)
        return;
    dbus_message_append_args(sig, DBUS_TYPE_BOOLEAN, &v, DBUS_TYPE_INVALID);
    dbus_connection_send(conn, sig, nullptr);
    dbus_message_unref(sig);
}
