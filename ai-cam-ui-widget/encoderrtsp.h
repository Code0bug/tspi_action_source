#ifndef ENCODE_RTSP_H
#define ENCODE_RTSP_H

#include <QObject>
#include "rk_mpi.h"

class EncoderRTSP : public QObject
{
    Q_OBJECT
public:
    explicit EncoderRTSP(QObject *parent = nullptr);

signals:

};

#endif // ENCODE_RTSP_H
