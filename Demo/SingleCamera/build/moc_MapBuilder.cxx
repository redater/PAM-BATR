/****************************************************************************
** Meta object code from reading C++ file 'MapBuilder.h'
**
** Created: Sun Jul 26 21:08:04 2015
**      by: The Qt Meta Object Compiler version 63 (Qt 4.8.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../MapBuilder.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MapBuilder.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 63
#error "This file was generated using the moc from 4.8.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_MapBuilder[] = {

 // content:
       6,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: signature, parameters, type, tag, flags
      17,   12,   11,   11, 0x08,
      60,   54,   11,   11, 0x08,

       0        // eod
};

static const char qt_meta_stringdata_MapBuilder[] = {
    "MapBuilder\0\0data\0processOdometry(rtabmap::SensorData)\0"
    "stats\0processStatistics(rtabmap::Statistics)\0"
};

void MapBuilder::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Q_ASSERT(staticMetaObject.cast(_o));
        MapBuilder *_t = static_cast<MapBuilder *>(_o);
        switch (_id) {
        case 0: _t->processOdometry((*reinterpret_cast< const rtabmap::SensorData(*)>(_a[1]))); break;
        case 1: _t->processStatistics((*reinterpret_cast< const rtabmap::Statistics(*)>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObjectExtraData MapBuilder::staticMetaObjectExtraData = {
    0,  qt_static_metacall 
};

const QMetaObject MapBuilder::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_MapBuilder,
      qt_meta_data_MapBuilder, &staticMetaObjectExtraData }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &MapBuilder::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *MapBuilder::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *MapBuilder::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_MapBuilder))
        return static_cast<void*>(const_cast< MapBuilder*>(this));
    if (!strcmp(_clname, "UEventsHandler"))
        return static_cast< UEventsHandler*>(const_cast< MapBuilder*>(this));
    return QWidget::qt_metacast(_clname);
}

int MapBuilder::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    return _id;
}
QT_END_MOC_NAMESPACE
