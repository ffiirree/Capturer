#include "libcap/filter.h"

#include "libcap/hwaccel.h"
#include "logging.h"

#include <probe/defer.h>
#include <probe/util.h>

extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
}

namespace av::graph
{
    int create_video_src(AVFilterGraph *graph, AVFilterContext **ctx, const av::vformat_t& fmt,
                         const AVBufferRef *frames_ctx)
    {
        const auto buffer = avfilter_get_by_name("buffer");
        auto       par    = av_buffersrc_parameters_alloc();
        if (!par) {
            loge("[V] failed to alloc parameters of buffersrc.");
            return -1;
        }
        defer(av_freep(&par));

        *ctx = avfilter_graph_alloc_filter(graph, buffer, "video-source");
        if (!*ctx) {
            loge("[V] failed to alloc context of buffersrc.");
            return -1;
        }

        par->format              = fmt.pix_fmt;
        par->time_base           = fmt.time_base;
        par->frame_rate          = fmt.framerate;
        par->width               = fmt.width;
        par->height              = fmt.height;
        par->sample_aspect_ratio = fmt.sample_aspect_ratio;
        par->color_space         = fmt.color.space;
        par->color_range         = fmt.color.range;
        if (fmt.hwaccel != AV_HWDEVICE_TYPE_NONE && frames_ctx) {
            par->hw_frames_ctx = const_cast<AVBufferRef *>(frames_ctx);
        }

        if (av_buffersrc_parameters_set(*ctx, par) < 0) {
            loge("[V] failed to set parameters for buffersrc");
            return -1;
        }

        if (avfilter_init_dict(*ctx, nullptr) < 0) {
            loge("[V] failed to set options for buffersrc");
            return -1;
        }

        logi("[V] buffersrc    : '{}'", to_string(fmt));

        return 0;
    }

    int create_audio_src(AVFilterGraph *graph, AVFilterContext **ctx, const av::aformat_t& fmt)
    {
        const auto args = to_string(fmt);

        if (avfilter_graph_create_filter(ctx, avfilter_get_by_name("abuffer"), "audio-source", args.c_str(),
                                         nullptr, graph) < 0) {
            loge("[A] failed to create 'abuffer'.");
            return -1;
        }

        logi("[A] buffersrc : '{}'", args);

        return 0;
    }

    int create_video_sink(AVFilterGraph *graph, AVFilterContext **ctx, const av::vformat_t& args,
                          std::vector<AVPixelFormat> formats, std::vector<AVColorSpace> spaces,
                          std::vector<AVColorRange> ranges)
    {
        if (*ctx = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("buffersink"), "video-sink");
            !*ctx) {
            loge("[V] failed to alloc 'buffersink'.");
            return -1;
        }

        // pixel_formats (array of pixel formats)
        formats.emplace_back(args.pix_fmt);
        probe::util::unique(formats);
        if (av_opt_set_array(*ctx, "pixel_formats", AV_OPT_SEARCH_CHILDREN, 0,
                             static_cast<unsigned int>(formats.size()), AV_OPT_TYPE_PIXEL_FMT,
                             formats.data()) < 0) {
            loge("[V] failed to set 'pixel_formats' for 'buffersink'");
            return -1;
        }

        // colorspaces (array of int)
        spaces.emplace_back(args.color.space);
        probe::util::unique(spaces);
        if (av_opt_set_array(*ctx, "colorspaces", AV_OPT_SEARCH_CHILDREN, 0,
                             static_cast<unsigned int>(spaces.size()), AV_OPT_TYPE_INT,
                             spaces.data()) < 0) {
            loge("[V] failed to set 'colorspaces' for 'buffersink'");
            return -1;
        }

        // colorranges (array of int)
        ranges.emplace_back(args.color.range);
        probe::util::unique(ranges);
        if (av_opt_set_array(*ctx, "colorranges", AV_OPT_SEARCH_CHILDREN, 0,
                             static_cast<unsigned int>(ranges.size()), AV_OPT_TYPE_INT,
                             ranges.data()) < 0) {
            loge("[V] failed to set 'colorranges' for 'buffersink'");
            return -1;
        }

        if (avfilter_init_dict(*ctx, nullptr) < 0) {
            loge("[A] failed to create 'buffersink'.");
            return -1;
        }

        logi("[V] buffersink <<: '{}'", av::to_string(args));

        return 0;
    }

    int create_audio_sink(AVFilterGraph *graph, AVFilterContext **ctx, const av::aformat_t& args)
    {
        if (*ctx = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("abuffersink"), "audio-sink");
            !*ctx) {
            loge("[A] failed to alloc 'buffersink'.");
            return -1;
        }

        // sample_formats (array of sample formats)
        if (av_opt_set(*ctx, "sample_formats", av_get_sample_fmt_name(args.sample_fmt),
                       AV_OPT_SEARCH_CHILDREN) < 0) {
            loge("[A] failed to set 'sample_formats' option.");
            return -1;
        }

        // channel_layouts (array of channel layouts) If an option is not set, all corresponding formats
        // are accepted.
        if (av_opt_set_array(*ctx, "channel_layouts", AV_OPT_SEARCH_CHILDREN, 0, 1, AV_OPT_TYPE_CHLAYOUT,
                             &args.ch_layout) < 0) {
            loge("[A] failed to set 'channel_layouts' option.");
            return -1;
        }

        // samplerates (array of int),
        if (av_opt_set_array(*ctx, "samplerates", AV_OPT_SEARCH_CHILDREN, 0, 1, AV_OPT_TYPE_INT,
                             &args.sample_rate) < 0) {
            loge("[A] failed to set 'samplerates' option.");
            return -1;
        }

        if (avfilter_init_dict(*ctx, nullptr) < 0) {
            loge("[A] failed to create 'buffersink'.");
            return -1;
        }

        logi("[A] buffersink: '{}'", av::to_string(args));

        return 0;
    }

    av::vformat_t buffersink_get_video_format(const AVFilterContext *sink)
    {
        auto hwaccel   = AV_HWDEVICE_TYPE_NONE;
        auto sw_format = AV_PIX_FMT_NONE;
        if (const auto frames_ctx_buf = av_buffersink_get_hw_frames_ctx(sink); frames_ctx_buf) {
            const auto frames_ctx = reinterpret_cast<AVHWFramesContext *>(frames_ctx_buf->data);
            if (frames_ctx) {
                if (frames_ctx->device_ctx) {
                    hwaccel = frames_ctx->device_ctx->type;
                }
                sw_format = frames_ctx->sw_format;
            }
        }

        return {
            .width               = av_buffersink_get_w(sink),
            .height              = av_buffersink_get_h(sink),
            .pix_fmt             = static_cast<AVPixelFormat>(av_buffersink_get_format(sink)),
            .framerate           = av_buffersink_get_frame_rate(sink),
            .sample_aspect_ratio = av_buffersink_get_sample_aspect_ratio(sink),
            .time_base           = av_buffersink_get_time_base(sink),
            .color =
                av::vformat_t::color_t{
                    .space = av_buffersink_get_colorspace(sink),
                    .range = av_buffersink_get_color_range(sink),
                },
            .hwaccel    = hwaccel,
            .sw_pix_fmt = sw_format,
        };
    }

    av::aformat_t buffersink_get_audio_format(const AVFilterContext *sink)
    {
        AVChannelLayout ch_layout{};
        av_buffersink_get_ch_layout(sink, &ch_layout);

        return {
            .sample_rate = av_buffersink_get_sample_rate(sink),
            .sample_fmt  = static_cast<AVSampleFormat>(av_buffersink_get_format(sink)),
            .ch_layout   = ch_layout,
            .time_base   = av_buffersink_get_time_base(sink),
        };
    }
} // namespace av::graph