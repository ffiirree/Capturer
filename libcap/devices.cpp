#include "libcap/devices.h"

#include "logging.h"
#include "probe/defer.h"

#ifdef __linux__
#include "libcap/linux-pulse/linux-pulse.h"
#include "libcap/linux-v4l2/linux-v4l2.h"
#elif _WIN32
#include "libcap/win-dshow/win-dshow.h"
#include "libcap/win-wasapi/win-wasapi.h"
#endif

namespace av
{
    std::vector<device_t> cameras()
    {
#if _WIN32
        return dshow::video_devices();
#elif __linux__
        return v4l2::device_list();
#else
        return {};
#endif
    }

    std::vector<device_t> audio_sources()
    {
#if _WIN32
        return wasapi::endpoints(av::device_type_t::source);
#elif __linux__
        pulse::init();
        defer(pulse::unref());

        std::vector<device_t> list;
        for (const auto& dev : pulse::source_list()) {
            if (!any(dev.type & device_type_t::monitor)) {
                list.push_back(dev);
            }
        }
        return list;
#else
        return {};
#endif
    }

    std::vector<device_t> audio_sinks()
    {
#if _WIN32
        return wasapi::endpoints(av::device_type_t::sink);
#elif __linux__
        pulse::init();
        defer(pulse::unref());
        std::vector<device_t> list;
        for (const auto& dev : pulse::source_list()) {
            if (any(dev.type & device_type_t::monitor)) {
                list.push_back(dev);
            }
        }
        return list;
#else
        return {};
#endif
    }

    std::optional<device_t> default_audio_source()
    {
#if _WIN32
        return wasapi::default_endpoint(av::device_type_t::source);
#elif __linux__
        pulse::init();
        defer(pulse::unref());
        return pulse::default_source();
#else
        return {};
#endif
    }

    // default monitor of sink
    std::optional<device_t> default_audio_sink()
    {
#if _WIN32
        return wasapi::default_endpoint(av::device_type_t::sink);
#elif __linux__
        pulse::init();
        defer(pulse::unref());

        auto dev = pulse::default_sink();
        if (dev.has_value()) {
            dev->id += ".monitor";
            dev->name = "Monitor of " + dev->name;
            dev->type = device_type_t::audio | device_type_t::source | device_type_t::monitor;
        }

        return dev;
#else
        return {};
#endif
    }
} // namespace av
