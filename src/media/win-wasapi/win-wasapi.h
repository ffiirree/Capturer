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
    std::vector<avdevice_t> endpoints(avdevice_t::io_t io_type);
    std::optional<avdevice_t> default_endpoint(avdevice_t::io_t io_type);

    std::optional<avdevice_t> device_info(IMMDevice*);
}

#endif // _WIN32

#endif // !CAPTURER_ENUM_WASAPI_H
