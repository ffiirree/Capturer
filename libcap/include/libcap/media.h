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

    enum err_t : int
    {
        SUCCESS     = 0,
        FAILURE     = -1,
        unsupported = -2,
        nullpointer = -3,
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

    // convert <T> to string
#if LIBAVUTIL_VERSION_MAJOR >= 57
    inline std::string to_string(const AVChannelLayout& channel_layout)
    {
        char buffer[32]{ 0 };
        if (av_channel_layout_describe(&channel_layout, buffer, sizeof(buffer)) < 0) return {};
        return buffer;
    }
#endif

    inline std::string channel_layout_name(int channels, uint64_t channel_layout)
    {
#if LIBAVUTIL_VERSION_MAJOR >= 57
        return to_string(AV_CHANNEL_LAYOUT_MASK(channels, channel_layout));
#else
        char buffer[32]{ 0 };
        av_get_channel_layout_string(buffer, sizeof(buffer), channels, channel_layout);
        return buffer; // ATTENTION: make the std::string do not contain '\0'
#endif
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
        auto args = fmt::format("video_size={}x{}:pix_fmt={}:time_base={}/{}:pixel_aspect={}/{}", fmt.width,
                                fmt.height, to_string(fmt.pix_fmt), fmt.time_base.num, fmt.time_base.den,
                                fmt.sample_aspect_ratio.num, std::max(fmt.sample_aspect_ratio.den, 1));
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