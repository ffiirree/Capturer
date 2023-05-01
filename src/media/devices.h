#ifndef DEVICES_H
#define DEVICES_H

#include "enum.h"

#include <optional>
#include <string>
#include <vector>

namespace av
{
    enum class device_type_t
    {
        unknown = 0x00,
        source  = 0x01,
        sink    = 0x02,
        audio   = 0x10,
        video   = 0x20,

        data_flow_mask = 0x0f,

        ENABLE_BITMASK_OPERATORS()
    };

    struct device_t
    {
        std::string name{};        // utf-8
        std::string id{};          // utf-8
        std::string description{}; // utf-8
        device_type_t type{ device_type_t::unknown };
        uint64_t state{};
    };

    [[nodiscard]] inline bool is_sink(const device_t& dev) { return any(dev.type & device_type_t::sink); }
    [[nodiscard]] inline bool is_source(const device_t& dev)
    {
        return any(dev.type & device_type_t::source);
    }

    inline std::string to_string(device_type_t t)
    {
        // clang-format off
        switch(t) {
        case device_type_t::sink:                           return "sink";
        case device_type_t::source:                         return "source";

        case device_type_t::audio:                          return "audio";
        case device_type_t::video:                          return "video";

        case device_type_t::sink   | device_type_t::audio:  return "audio sink";
        case device_type_t::source | device_type_t::audio:  return "audio source";
        case device_type_t::sink   | device_type_t::video:  return "video sink";
        case device_type_t::source | device_type_t::video:  return "video source";
        default:                                            return "unknown";
        }
        // clang-format on
    }

    std::vector<device_t> cameras();

    std::vector<device_t> audio_sources();
    std::vector<device_t> audio_sinks();

    std::optional<device_t> default_camera();

    std::optional<device_t> default_audio_source();
    std::optional<device_t> default_audio_sink();
} // namespace av

#endif // !DEVICES_H
