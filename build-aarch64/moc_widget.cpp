/****************************************************************************
** Meta object code from reading C++ file 'widget.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../widget.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'widget.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_Widget_t {
    QByteArrayData data[24];
    char stringdata0[301];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_Widget_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_Widget_t qt_meta_stringdata_Widget = {
    {
QT_MOC_LITERAL(0, 0, 6), // "Widget"
QT_MOC_LITERAL(1, 7, 20), // "WidgetToStartCapture"
QT_MOC_LITERAL(2, 28, 0), // ""
QT_MOC_LITERAL(3, 29, 19), // "WidgetToStopCapture"
QT_MOC_LITERAL(4, 49, 21), // "WidgetToStartDecVideo"
QT_MOC_LITERAL(5, 71, 4), // "path"
QT_MOC_LITERAL(6, 76, 6), // "doShow"
QT_MOC_LITERAL(7, 83, 7), // "cv::Mat"
QT_MOC_LITERAL(8, 91, 3), // "img"
QT_MOC_LITERAL(9, 95, 13), // "switchToPage1"
QT_MOC_LITERAL(10, 109, 13), // "switchToPage2"
QT_MOC_LITERAL(11, 123, 13), // "switchToPage0"
QT_MOC_LITERAL(12, 137, 13), // "switchToPage5"
QT_MOC_LITERAL(13, 151, 13), // "updatePicture"
QT_MOC_LITERAL(14, 165, 11), // "updateVideo"
QT_MOC_LITERAL(15, 177, 11), // "first_frame"
QT_MOC_LITERAL(16, 189, 11), // "doShowVideo"
QT_MOC_LITERAL(17, 201, 9), // "stopVideo"
QT_MOC_LITERAL(18, 211, 13), // "showNextPhoto"
QT_MOC_LITERAL(19, 225, 13), // "showPrevPhoto"
QT_MOC_LITERAL(20, 239, 18), // "PicListItemClicked"
QT_MOC_LITERAL(21, 258, 16), // "QListWidgetItem*"
QT_MOC_LITERAL(22, 275, 4), // "item"
QT_MOC_LITERAL(23, 280, 20) // "VideoListItemClicked"

    },
    "Widget\0WidgetToStartCapture\0\0"
    "WidgetToStopCapture\0WidgetToStartDecVideo\0"
    "path\0doShow\0cv::Mat\0img\0switchToPage1\0"
    "switchToPage2\0switchToPage0\0switchToPage5\0"
    "updatePicture\0updateVideo\0first_frame\0"
    "doShowVideo\0stopVideo\0showNextPhoto\0"
    "showPrevPhoto\0PicListItemClicked\0"
    "QListWidgetItem*\0item\0VideoListItemClicked"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_Widget[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      16,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       3,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   94,    2, 0x06 /* Public */,
       3,    0,   95,    2, 0x06 /* Public */,
       4,    1,   96,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       6,    1,   99,    2, 0x0a /* Public */,
       9,    0,  102,    2, 0x0a /* Public */,
      10,    0,  103,    2, 0x0a /* Public */,
      11,    0,  104,    2, 0x0a /* Public */,
      12,    0,  105,    2, 0x0a /* Public */,
      13,    1,  106,    2, 0x0a /* Public */,
      14,    2,  109,    2, 0x0a /* Public */,
      16,    1,  114,    2, 0x0a /* Public */,
      17,    0,  117,    2, 0x0a /* Public */,
      18,    0,  118,    2, 0x0a /* Public */,
      19,    0,  119,    2, 0x0a /* Public */,
      20,    1,  120,    2, 0x0a /* Public */,
      23,    1,  123,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    5,

 // slots: parameters
    QMetaType::Void, 0x80000000 | 7,    8,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    5,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 7,    5,   15,
    QMetaType::Void, 0x80000000 | 7,    8,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 21,   22,
    QMetaType::Void, 0x80000000 | 21,   22,

       0        // eod
};

void Widget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<Widget *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->WidgetToStartCapture(); break;
        case 1: _t->WidgetToStopCapture(); break;
        case 2: _t->WidgetToStartDecVideo((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 3: _t->doShow((*reinterpret_cast< cv::Mat(*)>(_a[1]))); break;
        case 4: _t->switchToPage1(); break;
        case 5: _t->switchToPage2(); break;
        case 6: _t->switchToPage0(); break;
        case 7: _t->switchToPage5(); break;
        case 8: _t->updatePicture((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 9: _t->updateVideo((*reinterpret_cast< QString(*)>(_a[1])),(*reinterpret_cast< cv::Mat(*)>(_a[2]))); break;
        case 10: _t->doShowVideo((*reinterpret_cast< cv::Mat(*)>(_a[1]))); break;
        case 11: _t->stopVideo(); break;
        case 12: _t->showNextPhoto(); break;
        case 13: _t->showPrevPhoto(); break;
        case 14: _t->PicListItemClicked((*reinterpret_cast< QListWidgetItem*(*)>(_a[1]))); break;
        case 15: _t->VideoListItemClicked((*reinterpret_cast< QListWidgetItem*(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (Widget::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&Widget::WidgetToStartCapture)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (Widget::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&Widget::WidgetToStopCapture)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (Widget::*)(QString );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&Widget::WidgetToStartDecVideo)) {
                *result = 2;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject Widget::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_Widget.data,
    qt_meta_data_Widget,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *Widget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Widget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_Widget.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int Widget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 16)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 16;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 16)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 16;
    }
    return _id;
}

// SIGNAL 0
void Widget::WidgetToStartCapture()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void Widget::WidgetToStopCapture()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void Widget::WidgetToStartDecVideo(QString _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
