/****************************************************************************
** Meta object code from reading C++ file 'AudioManager.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../AudioManager.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'AudioManager.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_AudioManager_t {
    QByteArrayData data[17];
    char stringdata0[190];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_AudioManager_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_AudioManager_t qt_meta_stringdata_AudioManager = {
    {
QT_MOC_LITERAL(0, 0, 12), // "AudioManager"
QT_MOC_LITERAL(1, 13, 18), // "capFinishedToAudio"
QT_MOC_LITERAL(2, 32, 0), // ""
QT_MOC_LITERAL(3, 33, 7), // "pcmData"
QT_MOC_LITERAL(4, 41, 8), // "uint64_t"
QT_MOC_LITERAL(5, 50, 11), // "captureTsUs"
QT_MOC_LITERAL(6, 62, 12), // "startCapture"
QT_MOC_LITERAL(7, 75, 11), // "stopCapture"
QT_MOC_LITERAL(8, 87, 13), // "startPlayback"
QT_MOC_LITERAL(9, 101, 7), // "codecId"
QT_MOC_LITERAL(10, 109, 10), // "sampleRate"
QT_MOC_LITERAL(11, 120, 8), // "channels"
QT_MOC_LITERAL(12, 129, 9), // "extradata"
QT_MOC_LITERAL(13, 139, 22), // "playEncodedAudioPacket"
QT_MOC_LITERAL(14, 162, 10), // "packetData"
QT_MOC_LITERAL(15, 173, 3), // "pts"
QT_MOC_LITERAL(16, 177, 12) // "stopPlayback"

    },
    "AudioManager\0capFinishedToAudio\0\0"
    "pcmData\0uint64_t\0captureTsUs\0startCapture\0"
    "stopCapture\0startPlayback\0codecId\0"
    "sampleRate\0channels\0extradata\0"
    "playEncodedAudioPacket\0packetData\0pts\0"
    "stopPlayback"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_AudioManager[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       6,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    2,   44,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       6,    0,   49,    2, 0x0a /* Public */,
       7,    0,   50,    2, 0x0a /* Public */,
       8,    4,   51,    2, 0x0a /* Public */,
      13,    2,   60,    2, 0x0a /* Public */,
      16,    0,   65,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::QByteArray, 0x80000000 | 4,    3,    5,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int, QMetaType::Int, QMetaType::QByteArray,    9,   10,   11,   12,
    QMetaType::Void, QMetaType::QByteArray, QMetaType::LongLong,   14,   15,
    QMetaType::Void,

       0        // eod
};

void AudioManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<AudioManager *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->capFinishedToAudio((*reinterpret_cast< QByteArray(*)>(_a[1])),(*reinterpret_cast< uint64_t(*)>(_a[2]))); break;
        case 1: _t->startCapture(); break;
        case 2: _t->stopCapture(); break;
        case 3: _t->startPlayback((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2])),(*reinterpret_cast< int(*)>(_a[3])),(*reinterpret_cast< QByteArray(*)>(_a[4]))); break;
        case 4: _t->playEncodedAudioPacket((*reinterpret_cast< QByteArray(*)>(_a[1])),(*reinterpret_cast< qint64(*)>(_a[2]))); break;
        case 5: _t->stopPlayback(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (AudioManager::*)(QByteArray , uint64_t );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&AudioManager::capFinishedToAudio)) {
                *result = 0;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject AudioManager::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_AudioManager.data,
    qt_meta_data_AudioManager,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *AudioManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AudioManager::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_AudioManager.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int AudioManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void AudioManager::capFinishedToAudio(QByteArray _t1, uint64_t _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
