/****************************************************************************
** Meta object code from reading C++ file 'videodecsend.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../videodecsend.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'videodecsend.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_VideoDecSend_t {
    QByteArrayData data[14];
    char stringdata0[161];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_VideoDecSend_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_VideoDecSend_t qt_meta_stringdata_VideoDecSend = {
    {
QT_MOC_LITERAL(0, 0, 12), // "VideoDecSend"
QT_MOC_LITERAL(1, 13, 18), // "audioStreamStarted"
QT_MOC_LITERAL(2, 32, 0), // ""
QT_MOC_LITERAL(3, 33, 7), // "codecId"
QT_MOC_LITERAL(4, 41, 10), // "sampleRate"
QT_MOC_LITERAL(5, 52, 8), // "channels"
QT_MOC_LITERAL(6, 61, 9), // "extradata"
QT_MOC_LITERAL(7, 71, 16), // "audioPacketReady"
QT_MOC_LITERAL(8, 88, 10), // "packetData"
QT_MOC_LITERAL(9, 99, 3), // "pts"
QT_MOC_LITERAL(10, 103, 19), // "audioStreamFinished"
QT_MOC_LITERAL(11, 123, 13), // "do_send_frame"
QT_MOC_LITERAL(12, 137, 4), // "path"
QT_MOC_LITERAL(13, 142, 18) // "do_stop_send_frame"

    },
    "VideoDecSend\0audioStreamStarted\0\0"
    "codecId\0sampleRate\0channels\0extradata\0"
    "audioPacketReady\0packetData\0pts\0"
    "audioStreamFinished\0do_send_frame\0"
    "path\0do_stop_send_frame"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_VideoDecSend[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       3,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    4,   39,    2, 0x06 /* Public */,
       7,    2,   48,    2, 0x06 /* Public */,
      10,    0,   53,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
      11,    1,   54,    2, 0x0a /* Public */,
      13,    0,   57,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Int, QMetaType::Int, QMetaType::Int, QMetaType::QByteArray,    3,    4,    5,    6,
    QMetaType::Void, QMetaType::QByteArray, QMetaType::LongLong,    8,    9,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void, QMetaType::QString,   12,
    QMetaType::Void,

       0        // eod
};

void VideoDecSend::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<VideoDecSend *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->audioStreamStarted((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2])),(*reinterpret_cast< int(*)>(_a[3])),(*reinterpret_cast< QByteArray(*)>(_a[4]))); break;
        case 1: _t->audioPacketReady((*reinterpret_cast< QByteArray(*)>(_a[1])),(*reinterpret_cast< qint64(*)>(_a[2]))); break;
        case 2: _t->audioStreamFinished(); break;
        case 3: _t->do_send_frame((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 4: _t->do_stop_send_frame(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (VideoDecSend::*)(int , int , int , QByteArray );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VideoDecSend::audioStreamStarted)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (VideoDecSend::*)(QByteArray , qint64 );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VideoDecSend::audioPacketReady)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (VideoDecSend::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VideoDecSend::audioStreamFinished)) {
                *result = 2;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject VideoDecSend::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_VideoDecSend.data,
    qt_meta_data_VideoDecSend,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *VideoDecSend::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *VideoDecSend::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_VideoDecSend.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int VideoDecSend::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void VideoDecSend::audioStreamStarted(int _t1, int _t2, int _t3, QByteArray _t4)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t4))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void VideoDecSend::audioPacketReady(QByteArray _t1, qint64 _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void VideoDecSend::audioStreamFinished()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
