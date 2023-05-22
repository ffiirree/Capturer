#ifndef CAPTURER_WIN_DSHOW_H
#define CAPTURER_WIN_DSHOW_H

#ifdef _WIN32

#include "devices.h"

#include <optional>
#include <string>
#include <vector>

namespace dshow
{
    std::vector<av::device_t> video_devices();
    std::vector<av::device_t> audio_devices();
} // namespace dshow

#endif // !_WIN32

#endif // !CAPTURER_WIN_DSHOW_H