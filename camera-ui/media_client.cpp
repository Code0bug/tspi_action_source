#include "media_client.h"

#include <media_service.h>

#include <cstdio>
#include <cstring>

#include <QSocketNotifier>
#include <QString>

#include <dbus/dbus.h>

static const char kSignalMatch[] =
    "type='signal',interface='" MEDIA_SERVICE_INTERFACE "'";

struct MediaClient::Impl {
    DBusConnection *conn = nullptr;
};

DBusHandlerResult MediaClient::onSignal(DBusConnection *, DBusMessage *msg,
                                        void *user)
{
    if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    if (!dbus_message_has_interface(msg, MEDIA_SERVICE_INTERFACE))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    auto *self = static_cast<MediaClient *>(user);
    const char *member = dbus_message_get_member(msg);
    if (member != nullptr &&
        (std::strcmp(member, MEDIA_SERVICE_SIGNAL_PHOTO_SAVED) == 0 ||
         std::strcmp(member, MEDIA_SERVICE_SIGNAL_VIDEO_SAVED) == 0)) {
        const char *path = nullptr;
        if (dbus_message_get_args(msg, nullptr,
                                  DBUS_TYPE_STRING, &path,
                                  DBUS_TYPE_INVALID) &&
            path != nullptr) {
            const QString qpath = QString::fromUtf8(path);
            if (std::strcmp(member, MEDIA_SERVICE_SIGNAL_PHOTO_SAVED) == 0)
                emit self->photoSaved(qpath);
            else
                emit self->videoSaved(qpath);
        }
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    self->handleSignal(member);
    return DBUS_HANDLER_RESULT_HANDLED;
}

MediaClient::MediaClient(QObject *parent)
    : QObject(parent)
    , impl_(new Impl)
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
    dbus_bus_add_match(impl_->conn, kSignalMatch, &err);
    if (dbus_error_is_set(&err)) {
        std::fprintf(stderr, "MediaClient: add match: %s\n", err.message);
        dbus_error_free(&err);
    }
    dbus_connection_add_filter(impl_->conn, &MediaClient::onSignal, this, nullptr);

    int fd = -1;
    if (dbus_connection_get_unix_fd(impl_->conn, &fd)) {
        notifier_ = new QSocketNotifier(fd, QSocketNotifier::Read, this);
        connect(notifier_, &QSocketNotifier::activated, this, [this]() {
            dispatch();
        });
    }
}

MediaClient::~MediaClient()
{
    if (impl_->conn == nullptr)
        return;
    dbus_connection_remove_filter(impl_->conn, &MediaClient::onSignal, this);
    dbus_bus_remove_match(impl_->conn, kSignalMatch, nullptr);
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

void MediaClient::handleSignal(const char *member)
{
    if (member == nullptr)
        return;
    if (std::strcmp(member, MEDIA_SERVICE_SIGNAL_RECORD_STARTED) == 0)
        emit recordStarted();
    else if (std::strcmp(member, MEDIA_SERVICE_SIGNAL_RECORD_STOPPED) == 0)
        emit recordStopped();
    else if (std::strcmp(member, MEDIA_SERVICE_SIGNAL_PHOTO_STARTED) == 0)
        emit photoStarted();
}

void MediaClient::dispatch()
{
    if (impl_->conn == nullptr)
        return;
    dbus_connection_read_write(impl_->conn, 0);
    while (dbus_connection_dispatch(impl_->conn) == DBUS_DISPATCH_DATA_REMAINS)
        ;
}

bool MediaClient::playVideo(const QString &path)
{
    return call(MEDIA_SERVICE_METHOD_PLAY_VIDEO, path.toUtf8().constData());
}

bool MediaClient::call(const char *method, const char *arg)
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

    if (arg != nullptr &&
        !dbus_message_append_args(msg, DBUS_TYPE_STRING, &arg, DBUS_TYPE_INVALID)) {
        std::fprintf(stderr, "MediaClient: %s: append args failed\n", method);
        dbus_message_unref(msg);
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
