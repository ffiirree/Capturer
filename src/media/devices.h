#ifndef DEVICES_H
#define DEVICES_H

#include <QString>

class Devices {
public:
    static QList<QString> cameras();

    static QList<QString> microphones();

    static QList<QString> speakers();
};

#endif // DEVICES_H
