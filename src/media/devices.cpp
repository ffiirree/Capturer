#include "devices.h"

#include "defer.h"
#include "logging.h"
#include "utils.h"

//#include <QSet>

#ifdef __linux__

#include "linux-pulse/linux-pulse.h"
#include "linux-v4l2/linux-v4l2.h"

#elif _WIN32
#include "win-dshow/win-dshow.h"
#include "win-wasapi/win-wasapi.h"
#endif

namespace av
{
    std::vector<device_t> cameras()
    {
#if _WIN32
        return dshow::video_devices();
#endif
    }

    std::vector<device_t> audio_sources()
    {
#if _WIN32
        return wasapi::endpoints(av::device_type_t::source);
#endif
    }

    std::vector<device_t> audio_sinks()
    {
#if _WIN32
        return wasapi::endpoints(av::device_type_t::sink);
#endif
    }

    std::optional<device_t> default_camera() { return std::nullopt; }

    std::optional<device_t> default_audio_source()
    {
#if _WIN32
        return wasapi::default_endpoint(av::device_type_t::source);
#endif
    }
    std::optional<device_t> default_audio_sink()
    {
#if _WIN32
        return wasapi::default_endpoint(av::device_type_t::sink);
#endif
    }
} // namespace av

//QList<QString> Devices::cameras()
//{
//    QSet<QString> cameras{};
//#ifdef __linux__
//    auto devices = v4l2_device_list();
//    for (auto& device : devices) {
//        cameras.insert(QString::fromStdString(device.name_));
//    }
//#elif _WIN32
//    for (const auto& dev : dshow::video_devices()) {
//        cameras.insert(QString::fromUtf8(dev.name.c_str()));
//    }
//#endif
//
//    return cameras.values();
//}
//
//QList<QString> Devices::microphones()
//{
//    QSet<QString> microphones{};
//#ifdef __linux__
//    pulse_init();
//    auto sources = pulse_get_source_info_list();
//    for (auto& s : sources) {
//        if (!s.is_monitor_) microphones.insert(QString::fromStdString(s.name_));
//    }
//    pulse_unref();
//#elif _WIN32
//    for (const auto& dev : wasapi::endpoints(av::device_type_t::source)) {
//        microphones.insert(QString::fromUtf8(dev.name.c_str()));
//    }
//#endif
//    return microphones.values();
//}
//
//QList<QString> Devices::speakers()
//{
//    QSet<QString> speakers{};
//
//#ifdef __linux__
//    pulse_init();
//    auto sources = pulse_get_source_info_list();
//    for (auto& s : sources) {
//        if (s.is_monitor_) speakers.insert(QString::fromStdString(s.name_));
//    }
//    pulse_unref();
//#elif _WIN32
//    for (const auto& dev : wasapi::endpoints(av::device_type_t::sink)) {
//        speakers.insert(QString::fromUtf8(dev.name.c_str()));
//    }
//#endif
//    return speakers.values();
//}
//
//QString Devices::default_audio_sink()
//{
//#ifdef __linux__
//    PulseServerInfo info;
//
//    pulse_init();
//    defer(pulse_unref());
//
//    if (pulse_get_server_info(info) < 0) {
//        LOG(ERROR) << "failed to get pulse server";
//        return {};
//    }
//    return QString::fromStdString(info.default_sink_ + ".monitor");
//#elif _WIN32
//    auto dft = wasapi::default_endpoint(av::device_type_t::sink);
//    if (dft.has_value()) return QString::fromUtf8(dft.value().name.c_str());
//
//    return {};
//#endif
//}
//
//QString Devices::default_audio_source()
//{
//#ifdef __linux__
//    PulseServerInfo info;
//
//    pulse_init();
//    defer(pulse_unref());
//
//    if (pulse_get_server_info(info) < 0) {
//        LOG(ERROR) << "failed to get pulse server";
//        return {};
//    }
//    return QString::fromStdString(info.default_source_);
//#elif _WIN32
//    auto dft = wasapi::default_endpoint(av::device_type_t::source);
//    if (dft.has_value()) return QString::fromUtf8(dft.value().name.c_str());
//
//    return {};
//#endif
//}