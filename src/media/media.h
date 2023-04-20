#ifndef CAPTURER_MEDIA_H
#define CAPTURER_MEDIA_H

#include "clock.h"
extern "C" {
#include <libavutil/pixfmt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/rational.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
}

struct vformat_t {
    int width{ 0 };
    int height{ 0 };

    enum AVPixelFormat pix_fmt { AV_PIX_FMT_NONE };
    AVRational framerate{ 24, 1 };
    AVRational sample_aspect_ratio{ 1,1 };
    AVRational time_base{ 1, OS_TIME_BASE };

    enum AVHWDeviceType hwaccel { AV_HWDEVICE_TYPE_NONE };
};

struct aformat_t {
    int sample_rate{ 48000 };
    int channels{ 2 };
    enum AVSampleFormat sample_fmt { AV_SAMPLE_FMT_NONE };
    uint64_t channel_layout{ 0 };

    AVRational time_base{ 1, OS_TIME_BASE };
};

inline std::string channel_layout_name(int channels, int channel_layout)
{
    std::string buffer{ 8, {} };
    av_get_channel_layout_string(&(buffer.data()[0]), 8, channels, channel_layout);
    return buffer;
}

inline std::string pix_fmt_name(AVPixelFormat fmt)
{
    auto cstr = av_get_pix_fmt_name(fmt);
    if (cstr == nullptr) {
        return "none";
    }
    return cstr;
}

inline std::string sample_fmt_name(AVSampleFormat fmt)
{
    auto cstr = av_get_sample_fmt_name(fmt);
    if (cstr == nullptr) {
        return "none";
    }
    return cstr;
}

#endif // !CAPTURER_MEDIA_H