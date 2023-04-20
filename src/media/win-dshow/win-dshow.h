#ifndef CAPTURER_WIN_DSHOW_H
#define CAPTURER_WIN_DSHOW_H

#ifdef _WIN32

#include <vector>
#include <optional>
#include <string>
#include "devices.h"

namespace dshow
{
    std::vector<avdevice_t> video_devices();
    std::vector<avdevice_t> audio_devices();
}

#endif // !_WIN32

#endif // !CAPTURER_WIN_DSHOW_H