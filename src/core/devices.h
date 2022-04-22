#ifndef DEVICES_H
#define DEVICES_H

#include "utils.h"
#include <QString>

class Devices {
public:
    static QList<QString> cameras();

    static QList<QString> microphones();

    static QList<QString> speakers();

    static QString defaultMicrophone();

    static QString defaultSpeaker();
};

#endif // DEVICES_H
