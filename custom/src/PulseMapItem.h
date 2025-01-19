#pragma once

#include <QObject>

class PulseMapItem : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString          url             MEMBER _url             CONSTANT)
    Q_PROPERTY(QGeoCoordinate   coordinate      MEMBER _coordinate      CONSTANT)
    Q_PROPERTY(uint             tagId           MEMBER _tagId           CONSTANT)
    Q_PROPERTY(double           snr             MEMBER _snr             CONSTANT)
    Q_PROPERTY(double           antennaDegrees  MEMBER _antennaDegrees  CONSTANT)   // -1 indicates monopole antenna

public:
    PulseMapItem(QUrl& itemUrl, QGeoCoordinate coordinate, uint tagId, double snr, double antennaDegrees, QObject* parent)
        : QObject           (parent)
        , _url              (itemUrl.toString())
        , _coordinate       (coordinate)
        , _tagId            (tagId)
        , _snr              (snr)
        , _antennaDegrees   (antennaDegrees)
    { }

private:
    QString         _url;
    QGeoCoordinate  _coordinate;
    uint            _tagId;
    double          _snr;
    double          _antennaDegrees;
};
