#ifndef DEVICES_H
#define DEVICES_H

#include <QString>

class Devices {
public:
    static QList<QString> cameras();

    static QList<QString> microphones();

    static QList<QString> speakers();

    static QString default_audio_sink();
    static QString default_audio_source();
};

#endif // DEVICES_H
