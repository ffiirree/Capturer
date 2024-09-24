#include "libcap/filter.h"

#include "logging.h"
extern "C" {
#include <libavutil/opt.h>
}

namespace av::graph
{
    int create_video_src(AVFilterGraph *graph, AVFilterContext **ctx, const av::vformat_t& fmt)
    {
        const auto args = to_string(fmt);

        if (avfilter_graph_create_filter(ctx, avfilter_get_by_name("buffer"), "video-source", args.c_str(),
                                         nullptr, graph) < 0) {
            loge("[V] failed to create 'buffer'.");
            return -1;
        }

        logi("[V] buffersrc : '{}'", args);

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

    int create_video_sink(AVFilterGraph *graph, AVFilterContext **ctx, const av::vformat_t& args)
    {
        if (avfilter_graph_create_filter(ctx, avfilter_get_by_name("buffersink"), "video-sink", nullptr,
                                         nullptr, graph) < 0) {
            loge("[V] failed to create 'buffersink'.");
            return -1;
        }

        // pix_fmts (int list),
        const AVPixelFormat pix_fmts[] = { args.pix_fmt, AV_PIX_FMT_NONE };
        if (av_opt_set_int_list(*ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN) < 0) {
            loge("[V] failed to set 'pix_fmts' for 'buffersink'");
            return -1;
        }

        // color_spaces (int list)
        const AVColorSpace spaces[] = { args.color.space, AVCOL_SPC_UNSPECIFIED };
        if (av_opt_set_int_list(*ctx, "color_spaces", spaces, AVCOL_SPC_UNSPECIFIED,
                                AV_OPT_SEARCH_CHILDREN) < 0) {
            loge("[V] failed to set 'color_spaces' for 'buffersink'");
            return -1;
        }

        // color_ranges (int list)
        const AVColorRange ranges[] = { args.color.range, AVCOL_RANGE_UNSPECIFIED };
        if (av_opt_set_int_list(*ctx, "color_ranges", ranges, AVCOL_RANGE_UNSPECIFIED,
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
} // namespace av::graph