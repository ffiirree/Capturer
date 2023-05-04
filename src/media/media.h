#ifndef CAPTURER_MEDIA_H
#define CAPTURER_MEDIA_H

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
#include <libavutil/samplefmt.h>
}
#include "clock.h"
#include "fmt/format.h"

namespace av
{
    enum class vsync_t
    {
        cfr,
        vfr,
    };

    struct vformat_t
    {
        int width{ 0 };
        int height{ 0 };

        AVPixelFormat pix_fmt{ AV_PIX_FMT_NONE };
        AVRational framerate{ 24, 1 };
        AVRational sample_aspect_ratio{ 1, 1 };
        AVRational time_base{ 1, OS_TIME_BASE };

        AVHWDeviceType hwaccel{ AV_HWDEVICE_TYPE_NONE };
    };

    struct aformat_t
    {
        int sample_rate{ 48000 };
        int channels{ 2 };
        AVSampleFormat sample_fmt{ AV_SAMPLE_FMT_NONE };
        uint64_t channel_layout{ 0 };

        AVRational time_base{ 1, OS_TIME_BASE };
    };

    struct frame
    {
        frame() { ptr_ = av_frame_alloc(); }
        frame(const frame& other)
        {
            ptr_  = av_frame_alloc();
            *this = other;
        }
        frame(frame&& other) noexcept
        {
            ptr_  = av_frame_alloc();
            *this = std::forward<frame>(other);
        }
        ~frame() { av_frame_free(&ptr_); }

        frame& operator=(const frame& other)
        {
            unref();
            ref(other);
            return *this;
        }

        frame& operator=(const AVFrame* other)
        {
            unref();
            ref(other);
            return *this;
        }

        frame& operator=(frame&& other) noexcept
        {
            if (this != &other) 
            {
                unref();
                av_frame_move_ref(ptr_, other.ptr_);
            }
            return *this;
        }

        auto get() const noexcept { return ptr_; }
        auto put()
        {
            unref();
            return ptr_;
        }

        void unref() { av_frame_unref(ptr_); }
        int ref(const frame& other) { return av_frame_ref(ptr_, other.get()); }
        int ref(const AVFrame *other) { return av_frame_ref(ptr_, other); }

        AVFrame *operator->() const noexcept { return ptr_; }

    private:
        AVFrame *ptr_{ nullptr };
    };

    inline std::string channel_layout_name(int channels, uint64_t channel_layout)
    {
        char buffer[32]{ 0 };
        av_get_channel_layout_string(buffer, sizeof(buffer), channels, channel_layout);
        return buffer; // ATTENTION: make the std::string do not contain '\0'
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
        return str ? static_cast<char>(std::toupper(to_string(type)[0])) : '-';
    }

    inline std::string to_string(AVHWDeviceType type)
    {
        auto str = av_hwdevice_get_type_name(type);
        return str ? str : std::string{};
    }

    inline std::string to_string(const AVRational& r) { return fmt::format("{}/{}", r.num, r.den); }

    // for creating vbuffer src
    inline std::string to_string(const vformat_t& fmt)
    {
        return fmt::format(
            "video_size={}x{}:pix_fmt={}:time_base={}/{}:pixel_aspect={}/{}:frame_rate={}/{}", fmt.width,
            fmt.height, to_string(fmt.pix_fmt), fmt.time_base.num, fmt.time_base.den,
            fmt.sample_aspect_ratio.num, fmt.sample_aspect_ratio.den, fmt.framerate.num, fmt.framerate.den);
    }

    // for creating abuffer src
    inline std::string to_string(const aformat_t& fmt)
    {
        return fmt::format("sample_rate={}:sample_fmt={}:channels={}:channel_layout={}:time_base={}/{}",
                           fmt.sample_rate, to_string(fmt.sample_fmt), fmt.channels,
                           channel_layout_name(fmt.channels, fmt.channel_layout), fmt.time_base.num,
                           fmt.time_base.den);
    }

    inline std::string to_string(vsync_t m)
    {
        switch (m) {
        case vsync_t::cfr: return "CFR"; break;
        case vsync_t::vfr: return "VFR"; break;
        default: return "unknown";
        }
    }

    inline vsync_t to_vsync(const std::string& str)
    {
        if (str == "vfr" || str == "VFR") return vsync_t::vfr;

        return vsync_t::cfr;
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

#endif // !CAPTURER_MEDIA_H