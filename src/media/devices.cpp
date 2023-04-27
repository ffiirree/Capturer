#include "devices.h"

#include "defer.h"
#include "logging.h"
#include "utils.h"

#include <QSet>

#ifdef __linux__

#include "linux-pulse/linux-pulse.h"
#include "linux-v4l2/linux-v4l2.h"

#elif _WIN32
#include "win-dshow/win-dshow.h"
#include "win-wasapi/win-wasapi.h"
#endif

QList<QString> Devices::cameras()
{
    QSet<QString> cameras{};
#ifdef __linux__
    auto devices = v4l2_device_list();
    for (auto& device : devices) {
        cameras.insert(QString::fromStdString(device.name_));
    }
#elif _WIN32
    for (const auto& dev : dshow::video_devices()) {
        cameras.insert(QString::fromUtf8(dev.name.c_str()));
    }
#endif

    return cameras.values();
}

QList<QString> Devices::microphones()
{
    QSet<QString> microphones{};
#ifdef __linux__
    pulse_init();
    auto sources = pulse_get_source_info_list();
    for (auto& s : sources) {
        if (!s.is_monitor_) microphones.insert(QString::fromStdString(s.name_));
    }
    pulse_unref();
#elif _WIN32
    for (const auto& dev : wasapi::endpoints(avdevice_t::SOURCE)) {
        microphones.insert(QString::fromUtf8(dev.name.c_str()));
    }
#endif
    return microphones.values();
}

QList<QString> Devices::speakers()
{
    QSet<QString> speakers{};

#ifdef __linux__
    pulse_init();
    auto sources = pulse_get_source_info_list();
    for (auto& s : sources) {
        if (s.is_monitor_) speakers.insert(QString::fromStdString(s.name_));
    }
    pulse_unref();
#elif _WIN32
    for (const auto& dev : wasapi::endpoints(avdevice_t::SINK)) {
        speakers.insert(QString::fromUtf8(dev.name.c_str()));
    }
#endif
    return speakers.values();
}

QString Devices::default_audio_sink()
{
#ifdef __linux__
    PulseServerInfo info;

    pulse_init();
    defer(pulse_unref());

    if (pulse_get_server_info(info) < 0) {
        LOG(ERROR) << "failed to get pulse server";
        return {};
    }
    return QString::fromStdString(info.default_sink_ + ".monitor");
#elif _WIN32
    auto dft = wasapi::default_endpoint(avdevice_t::SINK);
    if (dft.has_value()) return QString::fromUtf8(dft.value().name.c_str());

    return {};
#endif
}

QString Devices::default_audio_source()
{
#ifdef __linux__
    PulseServerInfo info;

    pulse_init();
    defer(pulse_unref());

    if (pulse_get_server_info(info) < 0) {
        LOG(ERROR) << "failed to get pulse server";
        return {};
    }
    return QString::fromStdString(info.default_source_);
#elif _WIN32
    auto dft = wasapi::default_endpoint(avdevice_t::SOURCE);
    if (dft.has_value()) return QString::fromUtf8(dft.value().name.c_str());

    return {};
#endif
}