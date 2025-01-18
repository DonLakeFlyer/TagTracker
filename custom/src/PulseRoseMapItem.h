#pragma once

#include <QObject>

class PulseRoseMapItem : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString          url             MEMBER _url             CONSTANT)
    Q_PROPERTY(int              rotationIndex   MEMBER _rotationIndex   CONSTANT)
    Q_PROPERTY(QGeoCoordinate   rotationCenter  MEMBER _rotationCenter  CONSTANT)

public:
    PulseRoseMapItem(QUrl& itemUrl, int rotationIndex, QGeoCoordinate rotationCenter, QObject* parent)
        : QObject(parent)
        , _url(itemUrl.toString())
        , _rotationIndex(rotationIndex)
        , _rotationCenter(rotationCenter)
    { }

private:
    QString         _url;
    int             _rotationIndex;
    QGeoCoordinate  _rotationCenter;
};
