#ifndef CAPTURER_DEVICES_H
#define CAPTURER_DEVICES_H

#include <cstdint>
#include <optional>
#include <probe/enum.h>
#include <string>
#include <vector>

namespace av
{
    enum class device_type_t
    {
        none    = 0x0000,
        unknown = none,
        source  = 0x0001,
        sink    = 0x0002,
        audio   = 0x0010,
        video   = 0x0020,
        monitor = 0x0100,

        data_flow_mask = 0x0f,

        ENABLE_BITMASK_OPERATORS()
    };

    enum class device_format_t
    {
        unsupported = 0x00,
        V4L2,
        PulseAudio,
        DirectShow,
        WASAPI,
        WindowsGraphicsCapture,
        MediaFoundation
    };

    struct device_t
    {
        std::string     name{};        // utf-8, linux: like 'UVC Camera (046d:081b)'
        std::string     id{};          // utf-8, linux: like '/dev/video0'
        std::string     description{}; // utf-8
        std::string     bus{};         // utf-8, bus info
        std::string     driver{};      // utf-8, driver name
        device_type_t   type{};
        device_format_t format{};
        uint64_t        state{};
    };

    [[nodiscard]] inline bool is_sink(const device_t& dev) { return any(dev.type & device_type_t::sink); }

    [[nodiscard]] inline bool is_source(const device_t& dev)
    {
        return any(dev.type & device_type_t::source);
    }

    inline std::string to_string(device_type_t t)
    {
        std::string str{};

        switch (t & static_cast<device_type_t>(0x00f0)) {
        case device_type_t::audio: str += "audio"; break;
        case device_type_t::video: str += "video"; break;
        default:                   break;
        }

        switch (t & static_cast<device_type_t>(0x000f)) {
        case device_type_t::sink:   str += "sink"; break;
        case device_type_t::source: str += "source"; break;
        default:                    break;
        }

        switch (t & static_cast<device_type_t>(0x0f00)) {
        case device_type_t::monitor: str += "(monitor)"; break;
        default:                     break;
        }

        if (str.empty()) str = "unknown";

        return str;
    }

    inline std::string to_string(const device_format_t fmt)
    {
        switch (fmt) {
        case device_format_t::V4L2:                   return "V4L2";
        case device_format_t::PulseAudio:             return "PulseAudio";
        case device_format_t::DirectShow:             return "DirectShow";
        case device_format_t::WASAPI:                 return "Windows Audio Session API";
        case device_format_t::WindowsGraphicsCapture: return "Windows Graphics Capture";
        case device_format_t::MediaFoundation:        return "Meida Foundation";
        default:                                      return "unsupported";
        }
    }

    std::vector<device_t> cameras();

    std::vector<device_t> audio_sources();
    std::vector<device_t> audio_sinks();

    std::optional<device_t> default_audio_source();
    std::optional<device_t> default_audio_sink();
} // namespace av

#endif //! CAPTURER_DEVICES_H
