#include "devices.h"
#include <QCameraInfo>
#include <QAudioDeviceInfo>

QList<QString> Devices::cameras()
{
    QSet<QString> cameras{};
    for (const auto& info : QCameraInfo::availableCameras()) {
        cameras.insert(info.description());
    }

    return cameras.values();
}

QList<QString> Devices::microphones()
{
    QSet<QString> microphones{};
    auto x = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
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
