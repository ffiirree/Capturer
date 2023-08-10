#ifndef CAPTURER_HWACCEL_H
#define CAPTURER_HWACCEL_H

#include <memory>
#include <optional>
#include <probe/defer.h>
#include <probe/library.h>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavutil/hwcontext.h>
}

namespace av
{
    namespace hwaccel
    {
        struct context_t
        {
            AVHWDeviceType type{ AV_HWDEVICE_TYPE_NONE };

            context_t(AVHWDeviceType dt, AVBufferRef *ctx)
                : type(dt),
                  device_ctx_(ctx)
            {}

            context_t(const context_t&)            = delete;
            context_t& operator=(const context_t&) = delete;
            context_t(context_t&&)                 = delete;
            context_t& operator=(context_t&&)      = delete;

            ~context_t()
            {
                if (!device_ctx_) av_buffer_unref(&device_ctx_);
                if (!frames_ctx_) av_buffer_unref(&frames_ctx_);
            }

            // device context
            AVBufferRef *device_ctx_ref() const { return av_buffer_ref(device_ctx_); }

            AVHWDeviceContext *device_ctx() const
            {
                return device_ctx_ ? reinterpret_cast<AVHWDeviceContext *>(device_ctx_->data) : nullptr;
            }

            template<typename T> T *native_device_ctx() const
            {
                return device_ctx() ? reinterpret_cast<T *>(device_ctx()->hwctx) : nullptr;
            }

            template<typename T1, typename T2> T2 *native_device() const
            {
                return native_device_ctx<T1>() ? reinterpret_cast<T2 *>(native_device_ctx<T1>()->device)
                                               : nullptr;
            }

            // frames context
            void frames_ctx_ref(AVBufferRef *ctx)
            {
                if (frames_ctx_) av_buffer_unref(&frames_ctx_);

                frames_ctx_ = av_buffer_ref(ctx);
            }

            auto frames_ctx_alloc()
            {
                frames_ctx_ = av_hwframe_ctx_alloc(device_ctx_);
                return frames_ctx_;
            }

            auto frames_ctx_init() { return av_hwframe_ctx_init(frames_ctx_); }

            AVBufferRef *frames_ctx_ref() const { return av_buffer_ref(frames_ctx_); }

            AVBufferRef *frames_ctx_buf() const { return frames_ctx_; }

            AVHWFramesContext *frames_ctx() const
            {
                return frames_ctx_ ? reinterpret_cast<AVHWFramesContext *>(frames_ctx_->data) : nullptr;
            }

        private:
            AVBufferRef *device_ctx_{ nullptr };
            AVBufferRef *frames_ctx_{ nullptr };
        };

        inline std::vector<std::shared_ptr<context_t>> contexts;

        inline std::shared_ptr<context_t> find_context(AVHWDeviceType hwtype)
        {
            for (auto& ctx : contexts) {
                if (ctx->type == hwtype) {
                    return ctx;
                }
            }

            return nullptr;
        }

        inline std::shared_ptr<context_t> create_context(AVHWDeviceType hwtype)
        {
            AVBufferRef *device_ref = nullptr;
            if (av_hwdevice_ctx_create(&device_ref, hwtype, nullptr, nullptr, 0) < 0) {
                return nullptr;
            }

            return contexts.emplace_back(std::make_shared<context_t>(hwtype, device_ref));
        }

        inline std::shared_ptr<context_t> get_context(AVHWDeviceType hwtype)
        {
            if (auto ctx = find_context(hwtype); ctx) return ctx;

            return create_context(hwtype);
        }

        inline std::shared_ptr<context_t> get_context(const std::string& name)
        {
            if (auto hw_type = av_hwdevice_find_type_by_name(name.c_str());
                hw_type != AV_HWDEVICE_TYPE_NONE) {
                get_context(hw_type);
            }

            return nullptr;
        }
    } // namespace hwaccel

    namespace hwaccel
    {
        inline bool is_supported(enum AVHWDeviceType type)
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
                case AV_HWDEVICE_TYPE_D3D11VA: return true;

                case AV_HWDEVICE_TYPE_VDPAU:
                case AV_HWDEVICE_TYPE_VAAPI:
                case AV_HWDEVICE_TYPE_QSV:
                case AV_HWDEVICE_TYPE_DRM:
                case AV_HWDEVICE_TYPE_OPENCL:
                case AV_HWDEVICE_TYPE_MEDIACODEC:
                // case AV_HWDEVICE_TYPE_VULKAN:
                case AV_HWDEVICE_TYPE_DXVA2:
                case AV_HWDEVICE_TYPE_VIDEOTOOLBOX:
                default: return false;
                }
            }
            return false;
        }

        inline std::vector<AVHWDeviceType> list_devices()
        {
            std::vector<AVHWDeviceType> ret;

            enum AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
            while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE) {
                ret.emplace_back(type);
            }
            return ret;
        }

        inline int setup_for_filter_graph(AVFilterGraph *graph, AVHWDeviceType type)
        {
            if (!graph || !is_supported(type)) return -1;

            auto ctx = get_context(type);
            if (!ctx) return -1;

            for (unsigned i = 0; i < graph->nb_filters; ++i) {
                graph->filters[i]->hw_device_ctx = ctx->device_ctx_ref();
            }

            return 0;
        }

        inline int set_frames_ctx_from_sink(AVFilterContext *sink, AVHWDeviceType hwtype)
        {
            if (!sink || !sink->hw_device_ctx) return -1;

            auto device_ctx = find_context(hwtype);
            auto frames_ctx = av_buffersink_get_hw_frames_ctx(sink);
            if (device_ctx && frames_ctx) {
                device_ctx->frames_ctx_ref(frames_ctx);
                return 0;
            }
            return -1;
        }

        inline int setup_for_encoding(AVCodecContext *codec_ctx, AVHWDeviceType type)
        {
            if (!codec_ctx || !is_supported(type)) return -1;

            auto device_ctx = get_context(type);
            if (device_ctx && device_ctx->device_ctx() && device_ctx->frames_ctx()) {
                codec_ctx->hw_frames_ctx = device_ctx->frames_ctx_ref(); // ref
                codec_ctx->hw_device_ctx = device_ctx->device_ctx_ref(); // ref
                return 0;
            }

            return -1;
        }

        inline int transfer_frame(AVFrame *input, enum AVPixelFormat pix_fmt)
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
    } // namespace hwaccel
} // namespace av
#endif //! CAPTURER_HWACCEL_H