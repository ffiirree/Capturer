#ifndef CAPTURER_ENUM_WASAPI_H
#define CAPTURER_ENUM_WASAPI_H

#ifdef _WIN32

#include <vector>
#include <optional>
#include <string>
#include <mmdeviceapi.h>
#include "devices.h"

namespace wasapi
{
    // all std::string is utf8
    std::vector<av::device_t> endpoints(av::device_type_t type);
    std::optional<av::device_t> default_endpoint(av::device_type_t type);

    std::optional<av::device_t> device_info(IMMDevice*);


    uint64_t to_ffmpeg_channel_layout(DWORD layout, int channels);
}

#endif // _WIN32

#endif // !CAPTURER_ENUM_WASAPI_H
