#ifndef CAPTURER_MEDIA_H
#define CAPTURER_MEDIA_H

extern "C" {
#include <libavutil/pixfmt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/rational.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
}
#include "clock.h"
#include "fmt/format.h"

namespace av 
{
    struct vformat_t
    {
        int width{ 0 };
        int height{ 0 };

        enum AVPixelFormat pix_fmt { AV_PIX_FMT_NONE };
        AVRational framerate{ 24, 1 };
        AVRational sample_aspect_ratio{ 1,1 };
        AVRational time_base{ 1, OS_TIME_BASE };

        enum AVHWDeviceType hwaccel { AV_HWDEVICE_TYPE_NONE };
    };

    struct aformat_t
    {
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

    inline std::string to_string(AVPixelFormat fmt)
    {
        auto str = av_get_pix_fmt_name(fmt);
        return str ? str : std::string{};
    }

    inline std::string to_string(AVSampleFormat fmt)
    {
        auto str = av_get_sample_fmt_name(fmt);
        return str ? str : std::string{};
    }

    inline std::string to_string(AVMediaType type)
    {
        auto str = av_get_media_type_string(type);
        return str ? str : std::string{};
    }

    inline char to_char(AVMediaType type)
    {
        auto str = av_get_media_type_string(type);
        return str ? std::toupper(to_string(type)[0]) : '-';
    }

    inline std::string to_string(AVHWDeviceType type)
    {
        auto str = av_hwdevice_get_type_name(type);
        return str ? str : std::string{};
    }

    inline std::string to_string(const AVRational& r)
    {
        return fmt::format("{}/{}", r.num, r.den);
    }

    // for creating vbuffer src
    inline std::string to_string(const vformat_t& fmt)
    {
        return fmt::format(
            "video_size={}x{}:pix_fmt={}:time_base={}/{}:pixel_aspect={}/{}:frame_rate={}/{}",
            fmt.width, fmt.height, to_string(fmt.pix_fmt),
            fmt.time_base.num, fmt.time_base.den,
            fmt.sample_aspect_ratio.num, fmt.sample_aspect_ratio.den,
            fmt.framerate.num, fmt.framerate.den
        );
    }

    // for creating abuffer src
    inline std::string to_string(const aformat_t& fmt)
    {
        return fmt::format(
            "sample_rate={}:sample_fmt={}:channels={}:channel_layout={}:time_base={}/{}",
            fmt.sample_rate, to_string(fmt.sample_fmt),
            fmt.channels, channel_layout_name(fmt.channels, fmt.channel_layout),
            fmt.time_base.num, fmt.time_base.den
        );
    }
}

template<>
struct fmt::formatter<AVRational>
{
    constexpr auto parse(fmt::format_parse_context& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const AVRational& r, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), "{}/{}", r.num, r.den);
    };
};

#endif // !CAPTURER_MEDIA_H