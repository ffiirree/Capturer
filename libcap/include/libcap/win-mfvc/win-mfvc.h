#ifndef CAPTURER_WIN_MFVC_H
#define CAPTURER_WIN_MFVC_H

#ifdef _WIN32

#include "libcap/devices.h"

#include <mfapi.h>
#include <vector>

extern "C" {
#include <libavcodec/codec_id.h>
#include <libavutil/pixfmt.h>
}

// Media Foundation Video Capture
namespace mfvc
{
    std::vector<av::device_t> video_devices();

    std::pair<AVCodecID, AVPixelFormat> to_ffmpeg_format(const GUID& fmt);
} // namespace mfvc

#endif

#endif //! CAPTURER_WIN_MFVC_H