#ifndef CAPTURER_FILTER_H
#define CAPTURER_FILTER_H

#include "media.h"
extern "C" {
#include <libavfilter/avfilter.h>
}

namespace av::graph
{
    int create_video_src(AVFilterGraph *graph, AVFilterContext **ctx, const av::vformat_t& args);
    int create_audio_src(AVFilterGraph *graph, AVFilterContext **ctx, const av::aformat_t& args);

    int create_video_sink(AVFilterGraph *graph, AVFilterContext **ctx, const av::vformat_t& args);
    int create_audio_sink(AVFilterGraph *graph, AVFilterContext **ctx, const av::aformat_t& args);

} // namespace av::graph

#endif //! CAPTURER_FILTER_H