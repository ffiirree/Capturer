#include "devices.h"
#include <QSet>

#ifdef __linux__

#include "linux-pulse/linux-pulse.h"
#include "linux-v4l2/linux-v4l2.h"

#elif _WIN32
#include "win-wasapi/enum-wasapi.h"
#include "win-dshow/enum-devices.h"
#endif

QList<QString> Devices::cameras() {
    QSet<QString> cameras{};
#ifdef __linux__
    auto devices = v4l2_device_list();
    for (auto &device: devices) {
        cameras.insert(QString::fromStdString(device.name_));
    }
#elif _WIN32
    for (const auto& [name, id] : enum_video_devices()) {
        cameras.insert(QString::fromStdWString(name));
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
#elif _WIN32
    for (const auto& [name, id] : enum_audio_endpoints(true)) {
        microphones.insert(QString::fromStdWString(name));
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
#elif _WIN32
    for (const auto& [name, id] : enum_audio_endpoints(false)) {
        speakers.insert(QString::fromStdWString(name));
    }
#endif
    return speakers.values();
}

