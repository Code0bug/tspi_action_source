#ifndef PHOTOMANAGEMENT_H
#define PHOTOMANAGEMENT_H

#include <QObject>
#include <QWidget>
#include <QFileSystemModel>
#include <QListWidgetItem>

class PhotoManagement : public QObject
{
    Q_OBJECT
public:
    explicit PhotoManagement(QObject *parent = nullptr);

signals:

};

#endif // PHOTOMANAGEMENT_H
