#ifndef CAPTURER_MEDIA_H
#define CAPTURER_MEDIA_H

#include "clock.h"

#include <fmt/format.h>
#include <map>
#include <probe/util.h>
#include <vector>

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
#include <libavutil/samplefmt.h>
}

namespace av
{
    enum class vsync_t
    {
        cfr,
        vfr,
    };

    enum status_t : int
    {
        OK = 0,

        // Negative Error Codes
        AGAIN       = -EAGAIN, // AVERROR(AGAIN)
        NOMEM       = -ENOMEM, // AVERROR(NOMEM)
        DENIED      = -EACCES,
        BAD_ADDRESS = -EFAULT,
        INVALID     = -EINVAL,
        ALREADY     = -EALREADY,
        UNSUPPORTED = -ENOTSUP,

        // Custom
        NULLPTR     = -1002,
        NOT_FOUND   = -1003,
        TIMEOUT     = -1005,
        END_OF_FILE = -1007,
        STOPPED     = -1008,
    };

    // video format options
    struct vformat_t
    {
        int width{ 0 };
        int height{ 0 };

        AVPixelFormat pix_fmt{ AV_PIX_FMT_NONE };
        AVRational    framerate{ 24, 1 };
        AVRational    sample_aspect_ratio{ 1, 1 };
        AVRational    time_base{ 1, OS_TIME_BASE };

        struct color_t
        {
            AVColorSpace                  space{ AVCOL_SPC_UNSPECIFIED };
            AVColorRange                  range{ AVCOL_RANGE_UNSPECIFIED };
            AVColorPrimaries              primaries{ AVCOL_PRI_UNSPECIFIED };
            AVColorTransferCharacteristic transfer{ AVCOL_TRC_UNSPECIFIED };
        } color{};

        AVHWDeviceType hwaccel{ AV_HWDEVICE_TYPE_NONE };
        AVPixelFormat  sw_pix_fmt{ AV_PIX_FMT_NONE };
    };

    // audio format options
    struct aformat_t
    {
        int            sample_rate{ 0 };
        AVSampleFormat sample_fmt{ AV_SAMPLE_FMT_NONE };

        int      channels{ 0 };
        uint64_t channel_layout{ 0 };

        AVRational time_base{ 1, OS_TIME_BASE };
    };

    inline uint64_t default_channel_layout(int channels)
    {
        AVChannelLayout ch{};
        av_channel_layout_default(&ch, channels);
        return ch.u.mask;
    }

    inline std::string to_string(const status_t code)
    {
        switch (code) {
        case OK:          return "OK";

        case AGAIN:       return "Resource Temporarily Unavailable";
        case NOMEM:       return "Cannot Allocate Memory";
        case DENIED:      return "Permission Denied";
        case BAD_ADDRESS: return "Bad Address";
        case INVALID:     return "Invalid Argument";
        case ALREADY:     return "Already Running";
        case UNSUPPORTED: return "Not Supported";

        // Custom
        case NULLPTR:     return "Null Pointer ";
        case NOT_FOUND:   return "Not Found";
        case TIMEOUT:     return "Timeout";
        case END_OF_FILE: return "End of File";
        case STOPPED:     return "Stopped";
        default:          return "Unknown";
        }
    }

    inline std::string ff_errstr(const int code)
    {
        char str[AV_ERROR_MAX_STRING_SIZE]{ 0 };
        return av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, code);
    }

    // convert <T> to string
    inline std::string to_string(const AVChannelLayout& channel_layout)
    {
        char buffer[32]{ 0 };
        if (av_channel_layout_describe(&channel_layout, buffer, sizeof(buffer)) < 0) return {};
        return buffer;
    }

    inline std::string channel_layout_name(int channels, uint64_t channel_layout)
    {
        return to_string(AV_CHANNEL_LAYOUT_MASK(channels, channel_layout));
    }

    inline std::string to_string(const AVPixelFormat fmt)
    {
        auto str = av_get_pix_fmt_name(fmt);
        return str ? str : std::string{};
    }

    inline std::string to_string(const AVColorSpace cp)
    {
        auto str = av_color_space_name(cp);
        return str ? str : std::string{};
    }

    inline std::string to_string(const AVColorRange cr)
    {
        auto str = av_color_range_name(cr);
        return str ? str : std::string{};
    }

    inline std::string to_string(const AVColorPrimaries cp)
    {
        auto str = av_color_primaries_name(cp);
        return str ? str : std::string{};
    }

    inline std::string to_string(const AVColorTransferCharacteristic trc)
    {
        auto str = av_color_transfer_name(trc);
        return str ? str : std::string{};
    }

    inline std::string to_string(const vformat_t::color_t color)
    {
        return fmt::format("space={}:primaries={}:transfer={}:range={}", to_string(color.space),
                           to_string(color.primaries), to_string(color.transfer), to_string(color.range));
    }

    inline std::string to_string(const AVSampleFormat fmt)
    {
        auto str = av_get_sample_fmt_name(fmt);
        return str ? str : std::string{};
    }

    inline std::string to_string(const AVMediaType type)
    {
        auto str = av_get_media_type_string(type);
        return str ? str : std::string{};
    }

    inline char to_char(const AVMediaType type)
    {
        const auto str = av_get_media_type_string(type);
        return str ? static_cast<char>(std::toupper(to_string(type)[0])) : '-';
    }

    inline std::string to_string(const AVHWDeviceType type)
    {
        auto str = av_hwdevice_get_type_name(type);
        return str ? str : std::string{};
    }

    inline std::string to_string(const AVRational& r) { return fmt::format("{}/{}", r.num, r.den); }

    // for creating vbuffer src
    inline std::string to_string(const vformat_t& fmt)
    {
        auto args = fmt::format(
            "video_size={}x{}:pix_fmt={}:time_base={}/{}:pixel_aspect={}/{}:colorspace={}:range={}",
            fmt.width, fmt.height, to_string(fmt.pix_fmt), fmt.time_base.num, fmt.time_base.den,
            fmt.sample_aspect_ratio.num, std::max(fmt.sample_aspect_ratio.den, 1),
            to_string(fmt.color.space), to_string(fmt.color.range));

        if (fmt.framerate.den && fmt.framerate.num) {
            args += fmt::format(":frame_rate={}/{}", fmt.framerate.num, fmt.framerate.den);
        }

        return args;
    }

    // for creating abuffer src
    inline std::string to_string(const aformat_t& fmt)
    {
        std::string repr =
            fmt::format("sample_rate={}:sample_fmt={}:channels={}:time_base={}/{}", fmt.sample_rate,
                        to_string(fmt.sample_fmt), fmt.channels, fmt.time_base.num, fmt.time_base.den);

        if (fmt.channel_layout)
            repr += ":channel_layout=" + channel_layout_name(fmt.channels, fmt.channel_layout);

        return repr;
    }

    inline std::string to_string(const vsync_t m)
    {
        switch (m) {
        case vsync_t::cfr: return "CFR";
        case vsync_t::vfr: return "VFR";
        default:           return "unknown";
        }
    }

    inline vsync_t to_vsync(const std::string& str)
    {
        if (str == "vfr" || str == "VFR") return vsync_t::vfr;

        return vsync_t::cfr;
    }

    inline std::vector<std::pair<std::string, std::string>> to_pairs(const AVDictionary *dict)
    {
        std::vector<std::pair<std::string, std::string>> kv{};

        auto entry = av_dict_get(dict, "", nullptr, AV_DICT_IGNORE_SUFFIX);
        for (; entry; entry = av_dict_get(dict, "", entry, AV_DICT_IGNORE_SUFFIX)) {
            kv.emplace_back(entry->key, entry->value);
        }
        return kv;
    }

    inline std::map<std::string, std::string> to_map(const AVDictionary *dict)
    {
        std::map<std::string, std::string> map{};

        auto entry = av_dict_get(dict, "", nullptr, AV_DICT_IGNORE_SUFFIX);
        for (; entry; entry = av_dict_get(dict, "", entry, AV_DICT_IGNORE_SUFFIX)) {
            map[entry->key] = entry->value;
        }
        return map;
    }
} // namespace av

template<> struct fmt::formatter<AVRational>
{
    constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template<typename FormatContext> auto format(const AVRational& r, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), "{}/{}", r.num, r.den);
    }
};

#endif //! CAPTURER_MEDIA_H