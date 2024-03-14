#include "libcap/hwaccel.h"

#include <probe/defer.h>
#include <probe/library.h>

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavfilter/buffersink.h>
}

namespace av::hwaccel
{
    static std::vector<std::shared_ptr<context_t>> contexts;

    std::shared_ptr<context_t> find_context(const AVHWDeviceType hwtype)
    {
        for (auto& ctx : contexts) {
            if (ctx->type == hwtype) {
                return ctx;
            }
        }

        return nullptr;
    }

    std::shared_ptr<context_t> create_context(AVHWDeviceType hwtype)
    {
        AVBufferRef *device_ref = nullptr;
        if (av_hwdevice_ctx_create(&device_ref, hwtype, nullptr, nullptr, 0) < 0) {
            return nullptr;
        }

        return contexts.emplace_back(std::make_shared<context_t>(hwtype, device_ref));
    }

    std::shared_ptr<context_t> get_context(const AVHWDeviceType hwtype)
    {
        if (auto ctx = find_context(hwtype); ctx) return ctx;

        return create_context(hwtype);
    }

    std::shared_ptr<context_t> get_context(const std::string& name)
    {
        if (const auto type = av_hwdevice_find_type_by_name(name.c_str()); type != AV_HWDEVICE_TYPE_NONE) {
            get_context(type);
        }

        return nullptr;
    }

    bool is_supported(const AVHWDeviceType type)
    {
        for (auto t = av_hwdevice_iterate_types(AV_HWDEVICE_TYPE_NONE); t != AV_HWDEVICE_TYPE_NONE;
             t      = av_hwdevice_iterate_types(t)) {
            if (t != type) continue;

            switch (type) {
            case AV_HWDEVICE_TYPE_CUDA:
#ifdef _WIN32
                return !!probe::library::load("nvcuda.dll");
#elif __linux__
                return !!probe::library::load("libcuda.so");
#else
                return false;
#endif
                // Windows, no need to check
            case AV_HWDEVICE_TYPE_D3D11VA:      return true;

            case AV_HWDEVICE_TYPE_VDPAU:
            case AV_HWDEVICE_TYPE_VAAPI:
            case AV_HWDEVICE_TYPE_QSV:
            case AV_HWDEVICE_TYPE_DRM:
            case AV_HWDEVICE_TYPE_OPENCL:
            case AV_HWDEVICE_TYPE_MEDIACODEC:
            // case AV_HWDEVICE_TYPE_VULKAN:
            case AV_HWDEVICE_TYPE_DXVA2:
            case AV_HWDEVICE_TYPE_VIDEOTOOLBOX:
            default:                            return false;
            }
        }
        return false;
    }

    std::vector<AVHWDeviceType> list_devices()
    {
        std::vector<AVHWDeviceType> ret;

        AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
        while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE) {
            ret.emplace_back(type);
        }
        return ret;
    }

    int setup_for_filter_graph(AVFilterGraph *graph, const AVHWDeviceType type)
    {
        if (!graph || !is_supported(type)) return -1;

        auto ctx = get_context(type);
        if (!ctx) return -1;

        for (unsigned i = 0; i < graph->nb_filters; ++i) {
            graph->filters[i]->hw_device_ctx = av_buffer_ref(ctx->device_ctx.get());
        }

        return 0;
    }

    int set_frames_ctx_from_sink(AVFilterContext *sink, const AVHWDeviceType hwtype)
    {
        if (!sink || !sink->hw_device_ctx) return -1;

        auto ctx        = find_context(hwtype);
        auto frames_ctx = av_buffersink_get_hw_frames_ctx(sink);
        if (ctx && frames_ctx) {
            ctx->frames_ctx = frames_ctx; // ref
            return 0;
        }
        return -1;
    }

    int setup_for_encoding(AVCodecContext *codec_ctx, AVHWDeviceType type)
    {
        if (!codec_ctx || !is_supported(type)) return -1;

        auto ctx = get_context(type);
        if (ctx && ctx->device_ctx && ctx->frames_ctx) {
            codec_ctx->hw_frames_ctx = av_buffer_ref(ctx->frames_ctx.get());
            codec_ctx->hw_device_ctx = av_buffer_ref(ctx->device_ctx.get());
            return 0;
        }

        return -1;
    }

    int transfer_frame(AVFrame *input, const AVPixelFormat pix_fmt)
    {
        if (input->format == pix_fmt) {
            // Nothing to do.
            return 0;
        }

        auto output = av_frame_alloc();
        if (!output) return AVERROR(ENOMEM);
        defer(av_frame_free(&output));

        output->format = pix_fmt;

        if (av_hwframe_transfer_data(output, input, 0) < 0) return -1;

        if (av_frame_copy_props(output, input) < 0) return -1;

        av_frame_unref(input);
        av_frame_move_ref(input, output);

        return 0;
    }
} // namespace av::hwaccel