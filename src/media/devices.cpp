#include "devices.h"
#include <QCameraInfo>
#include <QAudioDeviceInfo>

QList<QString> Devices::cameras()
{
    QSet<QString> cameras{};
    for (const auto& info : QCameraInfo::availableCameras()) {
#ifdef __linux__
        cameras.insert(info.deviceName());
#elif
        cameras.insert(info.description());
#endif
    }

    return cameras.values();
}

QList<QString> Devices::microphones()
{
    QSet<QString> microphones{};
    for (const auto& info : QAudioDeviceInfo::availableDevices(QAudio::AudioInput)) {
        microphones.insert(info.deviceName());
    }

    return microphones.values();
}

QString Devices::defaultMicrophone()
{
    return QAudioDeviceInfo::defaultInputDevice().deviceName();
}

QList<QString> Devices::speakers()
{
    QSet<QString> speakers{};
    for (const auto& info : QAudioDeviceInfo::availableDevices(QAudio::AudioOutput)) {
        speakers.insert(info.deviceName());
    }

    return speakers.values();
}

QString Devices::defaultSpeaker()
{
    return QAudioDeviceInfo::defaultOutputDevice().deviceName();
}
