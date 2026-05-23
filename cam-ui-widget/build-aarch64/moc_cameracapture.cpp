/****************************************************************************
** Meta object code from reading C++ file 'cameracapture.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../cameracapture.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'cameracapture.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_CameraCapture_t {
    QByteArrayData data[24];
    char stringdata0[272];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_CameraCapture_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_CameraCapture_t qt_meta_stringdata_CameraCapture = {
    {
QT_MOC_LITERAL(0, 0, 13), // "CameraCapture"
QT_MOC_LITERAL(1, 14, 15), // "capFinishedToUI"
QT_MOC_LITERAL(2, 30, 0), // ""
QT_MOC_LITERAL(3, 31, 7), // "cv::Mat"
QT_MOC_LITERAL(4, 39, 6), // "imgbuf"
QT_MOC_LITERAL(5, 46, 18), // "capFinishedToVideo"
QT_MOC_LITERAL(6, 65, 3), // "buf"
QT_MOC_LITERAL(7, 69, 5), // "FILE*"
QT_MOC_LITERAL(8, 75, 2), // "fp"
QT_MOC_LITERAL(9, 78, 8), // "uint64_t"
QT_MOC_LITERAL(10, 87, 3), // "pts"
QT_MOC_LITERAL(11, 91, 21), // "videoFinishedToUpdate"
QT_MOC_LITERAL(12, 113, 4), // "path"
QT_MOC_LITERAL(13, 118, 11), // "first_frame"
QT_MOC_LITERAL(14, 130, 24), // "videoFinishedToIMURecord"
QT_MOC_LITERAL(15, 155, 19), // "photoFinishedToSave"
QT_MOC_LITERAL(16, 175, 5), // "frame"
QT_MOC_LITERAL(17, 181, 18), // "videoFinishedToMP4"
QT_MOC_LITERAL(18, 200, 15), // "sigVideoMp4Init"
QT_MOC_LITERAL(19, 216, 12), // "startCapture"
QT_MOC_LITERAL(20, 229, 11), // "stopCapture"
QT_MOC_LITERAL(21, 241, 10), // "startVideo"
QT_MOC_LITERAL(22, 252, 9), // "stopVideo"
QT_MOC_LITERAL(23, 262, 9) // "takePhoto"

    },
    "CameraCapture\0capFinishedToUI\0\0cv::Mat\0"
    "imgbuf\0capFinishedToVideo\0buf\0FILE*\0"
    "fp\0uint64_t\0pts\0videoFinishedToUpdate\0"
    "path\0first_frame\0videoFinishedToIMURecord\0"
    "photoFinishedToSave\0frame\0videoFinishedToMP4\0"
    "sigVideoMp4Init\0startCapture\0stopCapture\0"
    "startVideo\0stopVideo\0takePhoto"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_CameraCapture[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      12,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       7,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   74,    2, 0x06 /* Public */,
       5,    3,   77,    2, 0x06 /* Public */,
      11,    2,   84,    2, 0x06 /* Public */,
      14,    1,   89,    2, 0x06 /* Public */,
      15,    2,   92,    2, 0x06 /* Public */,
      17,    0,   97,    2, 0x06 /* Public */,
      18,    1,   98,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
      19,    0,  101,    2, 0x0a /* Public */,
      20,    0,  102,    2, 0x0a /* Public */,
      21,    0,  103,    2, 0x0a /* Public */,
      22,    0,  104,    2, 0x0a /* Public */,
      23,    0,  105,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, 0x80000000 | 3, 0x80000000 | 7, 0x80000000 | 9,    6,    8,   10,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 3,   12,   13,
    QMetaType::Void, QMetaType::QString,   12,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 3,   12,   16,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   12,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void CameraCapture::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<CameraCapture *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->capFinishedToUI((*reinterpret_cast< cv::Mat(*)>(_a[1]))); break;
        case 1: _t->capFinishedToVideo((*reinterpret_cast< cv::Mat(*)>(_a[1])),(*reinterpret_cast< FILE*(*)>(_a[2])),(*reinterpret_cast< uint64_t(*)>(_a[3]))); break;
        case 2: _t->videoFinishedToUpdate((*reinterpret_cast< QString(*)>(_a[1])),(*reinterpret_cast< cv::Mat(*)>(_a[2]))); break;
        case 3: _t->videoFinishedToIMURecord((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 4: _t->photoFinishedToSave((*reinterpret_cast< QString(*)>(_a[1])),(*reinterpret_cast< cv::Mat(*)>(_a[2]))); break;
        case 5: _t->videoFinishedToMP4(); break;
        case 6: _t->sigVideoMp4Init((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 7: _t->startCapture(); break;
        case 8: _t->stopCapture(); break;
        case 9: _t->startVideo(); break;
        case 10: _t->stopVideo(); break;
        case 11: _t->takePhoto(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (CameraCapture::*)(cv::Mat );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CameraCapture::capFinishedToUI)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (CameraCapture::*)(cv::Mat , FILE * , uint64_t );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CameraCapture::capFinishedToVideo)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (CameraCapture::*)(QString , cv::Mat );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CameraCapture::videoFinishedToUpdate)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (CameraCapture::*)(QString );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CameraCapture::videoFinishedToIMURecord)) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (CameraCapture::*)(QString , cv::Mat );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CameraCapture::photoFinishedToSave)) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (CameraCapture::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CameraCapture::videoFinishedToMP4)) {
                *result = 5;
                return;
            }
        }
        {
            using _t = void (CameraCapture::*)(QString );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CameraCapture::sigVideoMp4Init)) {
                *result = 6;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject CameraCapture::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CameraCapture.data,
    qt_meta_data_CameraCapture,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *CameraCapture::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *CameraCapture::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CameraCapture.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int CameraCapture::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 12;
    }
    return _id;
}

// SIGNAL 0
void CameraCapture::capFinishedToUI(cv::Mat _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void CameraCapture::capFinishedToVideo(cv::Mat _t1, FILE * _t2, uint64_t _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void CameraCapture::videoFinishedToUpdate(QString _t1, cv::Mat _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void CameraCapture::videoFinishedToIMURecord(QString _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void CameraCapture::photoFinishedToSave(QString _t1, cv::Mat _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void CameraCapture::videoFinishedToMP4()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void CameraCapture::sigVideoMp4Init(QString _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
