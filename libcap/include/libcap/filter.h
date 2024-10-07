#ifndef CAPTURER_FILTER_H
#define CAPTURER_FILTER_H

#include "media.h"
extern "C" {
#include <libavfilter/avfilter.h>
}

namespace av::graph
{
    int create_video_src(AVFilterGraph *graph, AVFilterContext **ctx, const av::vformat_t& args,
                         const AVBufferRef *frames_ctx);
    int create_audio_src(AVFilterGraph *graph, AVFilterContext **ctx, const av::aformat_t& args);

    int create_video_sink(AVFilterGraph *graph, AVFilterContext **ctx, const av::vformat_t& args,
                          std::vector<AVPixelFormat> formats = {}, std::vector<AVColorSpace> spaces = {},
                          std::vector<AVColorRange> ranges = {});
    int create_audio_sink(AVFilterGraph *graph, AVFilterContext **ctx, const av::aformat_t& args);

    av::vformat_t buffersink_get_video_format(const AVFilterContext *sink);
    av::aformat_t buffersink_get_audio_format(const AVFilterContext *sink);
} // namespace av::graph

#endif //! CAPTURER_FILTER_H