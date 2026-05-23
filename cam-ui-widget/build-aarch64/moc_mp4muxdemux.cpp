/****************************************************************************
** Meta object code from reading C++ file 'mp4muxdemux.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../mp4muxdemux.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'mp4muxdemux.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_Mp4MuxDemux_t {
    QByteArrayData data[12];
    char stringdata0[131];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_Mp4MuxDemux_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_Mp4MuxDemux_t qt_meta_stringdata_Mp4MuxDemux = {
    {
QT_MOC_LITERAL(0, 0, 11), // "Mp4MuxDemux"
QT_MOC_LITERAL(1, 12, 10), // "initMp4Mux"
QT_MOC_LITERAL(2, 23, 0), // ""
QT_MOC_LITERAL(3, 24, 8), // "filename"
QT_MOC_LITERAL(4, 33, 11), // "closeMp4Mux"
QT_MOC_LITERAL(5, 45, 15), // "writeVideoToMp4"
QT_MOC_LITERAL(6, 61, 14), // "VENC_STREAM_S*"
QT_MOC_LITERAL(7, 76, 9), // "pstStream"
QT_MOC_LITERAL(8, 86, 15), // "writeAudioToMp4"
QT_MOC_LITERAL(9, 102, 7), // "pcmData"
QT_MOC_LITERAL(10, 110, 8), // "uint64_t"
QT_MOC_LITERAL(11, 119, 11) // "captureTsUs"

    },
    "Mp4MuxDemux\0initMp4Mux\0\0filename\0"
    "closeMp4Mux\0writeVideoToMp4\0VENC_STREAM_S*\0"
    "pstStream\0writeAudioToMp4\0pcmData\0"
    "uint64_t\0captureTsUs"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_Mp4MuxDemux[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    1,   34,    2, 0x0a /* Public */,
       4,    0,   37,    2, 0x0a /* Public */,
       5,    1,   38,    2, 0x0a /* Public */,
       8,    2,   41,    2, 0x0a /* Public */,

 // slots: parameters
    QMetaType::Bool, QMetaType::QString,    3,
    QMetaType::Void,
    QMetaType::Int, 0x80000000 | 6,    7,
    QMetaType::Int, QMetaType::QByteArray, 0x80000000 | 10,    9,   11,

       0        // eod
};

void Mp4MuxDemux::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<Mp4MuxDemux *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: { bool _r = _t->initMp4Mux((*reinterpret_cast< const QString(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 1: _t->closeMp4Mux(); break;
        case 2: { int _r = _t->writeVideoToMp4((*reinterpret_cast< VENC_STREAM_S*(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 3: { int _r = _t->writeAudioToMp4((*reinterpret_cast< QByteArray(*)>(_a[1])),(*reinterpret_cast< uint64_t(*)>(_a[2])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject Mp4MuxDemux::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_Mp4MuxDemux.data,
    qt_meta_data_Mp4MuxDemux,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *Mp4MuxDemux::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Mp4MuxDemux::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_Mp4MuxDemux.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int Mp4MuxDemux::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 4;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
