#ifndef CAPTURER_HWACCEL_H
#define CAPTURER_HWACCEL_H

#include "ffmpeg-wrapper.h"

#include <memory>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavutil/hwcontext.h>
}

namespace av::hwaccel
{
    struct context_t
    {
        context_t(AVHWDeviceType dt, AVBufferRef *ctx)
            : type(dt), device_ctx(ctx)
        {}

        AVHWDeviceType type{ AV_HWDEVICE_TYPE_NONE };

        av::buffer<AVHWDeviceContext> device_ctx{};
        av::buffer<AVHWFramesContext> frames_ctx{};

        //
        template<typename T> T *hwctx() const
        {
            return device_ctx.data() ? static_cast<T *>(device_ctx.data()->hwctx) : nullptr;
        }

        template<typename AVCTX> auto native_context() -> decltype(hwctx<AVCTX>()->device_context) const
        {
            return hwctx<AVCTX>() ? hwctx<AVCTX>()->device_context : nullptr;
        }

        template<typename AVCTX> auto native_device() -> decltype(hwctx<AVCTX>()->device) const
        {
            return hwctx<AVCTX>() ? hwctx<AVCTX>()->device : nullptr;
        }

        void frames_ctx_alloc()
        {
            if (!frames_ctx) frames_ctx.attach(av_hwframe_ctx_alloc(device_ctx.get()));
        }

        int frames_ctx_init() { return av_hwframe_ctx_init(frames_ctx.get()); }
    };

    std::shared_ptr<context_t> find_context(AVHWDeviceType hwtype);

    std::shared_ptr<context_t> create_context(AVHWDeviceType hwtype);

    std::shared_ptr<context_t> get_context(AVHWDeviceType hwtype);

    std::shared_ptr<context_t> get_context(const std::string& name);

    bool is_supported(AVHWDeviceType type);

    std::vector<AVHWDeviceType> list_devices();

    int setup_for_filter_graph(AVFilterGraph *graph, AVHWDeviceType type);

    int set_frames_ctx_from_sink(AVFilterContext *sink, AVHWDeviceType hwtype);

    int setup_for_encoding(AVCodecContext *codec_ctx, AVHWDeviceType type);

    int transfer_frame(AVFrame *input, AVPixelFormat pix_fmt);
} // namespace av::hwaccel
#endif //! CAPTURER_HWACCEL_H