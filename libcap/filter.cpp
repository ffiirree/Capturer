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

        logi("[V] buffersrc : '{}'", to_string(fmt));

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
        if (avfilter_graph_create_filter(ctx, avfilter_get_by_name("buffersink"), "video-sink", nullptr,
                                         nullptr, graph) < 0) {
            loge("[V] failed to create 'buffersink'.");
            return -1;
        }

        // pix_fmts (int list)
        formats.emplace_back(args.pix_fmt);
        probe::util::unique(formats);
        formats.emplace_back(AV_PIX_FMT_NONE);
        if (av_opt_set_int_list(*ctx, "pix_fmts", formats.data(), AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN) <
            0) {
            loge("[V] failed to set 'pix_fmts' for 'buffersink'");
            return -1;
        }

        // color_spaces (int list)
        spaces.emplace_back(args.color.space);
        probe::util::unique(spaces);
        spaces.emplace_back(AVCOL_SPC_UNSPECIFIED);
        if (av_opt_set_int_list(*ctx, "color_spaces", spaces.data(), AVCOL_SPC_UNSPECIFIED,
                                AV_OPT_SEARCH_CHILDREN) < 0) {
            loge("[V] failed to set 'color_spaces' for 'buffersink'");
            return -1;
        }

        // color_ranges (int list)
        ranges.emplace_back(args.color.range);
        probe::util::unique(ranges);
        ranges.emplace_back(AVCOL_RANGE_UNSPECIFIED);
        if (av_opt_set_int_list(*ctx, "color_ranges", ranges.data(), AVCOL_RANGE_UNSPECIFIED,
                                AV_OPT_SEARCH_CHILDREN) < 0) {
            loge("[V] failed to set 'color_ranges' for 'buffersink'");
            return -1;
        }

        logi("[V] buffersink: '{}'", av::to_string(args));

        return 0;
    }

    int create_audio_sink(AVFilterGraph *graph, AVFilterContext **ctx, const av::aformat_t& args)
    {
        if (avfilter_graph_create_filter(ctx, avfilter_get_by_name("abuffersink"), "audio-sink", nullptr,
                                         nullptr, graph) < 0) {
            loge("[A] failed to create 'buffersink'.");
            return -1;
        }

        // sample_fmts (int list)
        if (const AVSampleFormat sink_fmts[] = { args.sample_fmt, AV_SAMPLE_FMT_NONE };
            av_opt_set_int_list(*ctx, "sample_fmts", sink_fmts, AV_SAMPLE_FMT_NONE,
                                AV_OPT_SEARCH_CHILDREN) < 0) {
            loge("[A] failed to set 'sample_fmts' option.");
            return -1;
        }

        // channel_counts(int list)
        // all_channel_counts(bool)
        if (av_opt_set_int(*ctx, "all_channel_counts", 0, AV_OPT_SEARCH_CHILDREN) < 0) {
            loge("[A] failed to set 'all_channel_counts' option.");
            return -1;
        }

        // ch_layouts(string)
        const auto layout = av::channel_layout_name(args.channels, args.channel_layout);
        if (av_opt_set(*ctx, "ch_layouts", layout.c_str(), AV_OPT_SEARCH_CHILDREN) < 0) {
            loge("[A] failed to set 'channel_layouts' option.");
            return -1;
        }

        // sample_rates(int list)
        if (const int64_t sample_rates[] = { args.sample_rate, -1 };
            av_opt_set_int_list(*ctx, "sample_rates", sample_rates, -1, AV_OPT_SEARCH_CHILDREN) < 0) {
            loge("[A] failed to set 'sample_rates' option.");
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
            .sample_rate    = av_buffersink_get_sample_rate(sink),
            .sample_fmt     = static_cast<AVSampleFormat>(av_buffersink_get_format(sink)),
            .channels       = ch_layout.nb_channels,
            .channel_layout = ch_layout.u.mask,
            .time_base      = av_buffersink_get_time_base(sink),
        };
    }
} // namespace av::graph