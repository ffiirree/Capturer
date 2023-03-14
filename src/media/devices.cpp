#include "devices.h"
#include <QSet>

#ifdef __linux__

#include "linux-pulse/linux-pulse.h"
#include "linux-v4l2/linux-v4l2.h"

#elif
#include <QCameraInfo>
#include <QAudioDeviceInfo>
#endif

QList<QString> Devices::cameras() {
    QSet<QString> cameras{};
#ifdef __linux__
    auto devices = v4l2_device_list();
    for (auto &device: devices) {
        cameras.insert(QString::fromStdString(device.name_));
    }
#elif
    for (const auto& info : QCameraInfo::availableCameras()) {
        cameras.insert(info.description());
    }
#endif

    return cameras.values();
}

QList<QString> Devices::microphones() {
    QSet<QString> microphones{};
#ifdef  __linux__
    pulse_init();
    auto sources = pulse_get_source_info_list();
    for (auto &s: sources) {
        if (!s.is_monitor_)
            microphones.insert(QString::fromStdString(s.name_));
    }
    pulse_unref();
#elif
    for (const auto &info: QAudioDeviceInfo::availableDevices(QAudio::AudioInput)) {
        microphones.insert(info.deviceName());
    }
#endif
    return microphones.values();
}

QList<QString> Devices::speakers() {
    QSet<QString> speakers{};

#ifdef  __linux__
    pulse_init();
    auto sources = pulse_get_source_info_list();
    for (auto &s: sources) {
        if (s.is_monitor_)
            speakers.insert(QString::fromStdString(s.name_));
    }
    pulse_unref();
#elif
    for (const auto &info: QAudioDeviceInfo::availableDevices(QAudio::AudioOutput)) {
        speakers.insert(info.deviceName());
    }
#endif
    return speakers.values();
}

