#ifndef CAPTURER_HWACCEL_H
#define CAPTURER_HWACCEL_H

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
        AVHWDeviceType type{ AV_HWDEVICE_TYPE_NONE };

        context_t(AVHWDeviceType dt, AVBufferRef *ctx)
            : type(dt), device_ctx_(ctx)
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
        [[nodiscard]] AVBufferRef *device_ctx_ref() const { return av_buffer_ref(device_ctx_); }

        [[nodiscard]] AVHWDeviceContext *device_ctx() const
        {
            return device_ctx_ ? reinterpret_cast<AVHWDeviceContext *>(device_ctx_->data) : nullptr;
        }

        template<typename T> T *native_device_ctx() const
        {
            return device_ctx() ? static_cast<T *>(device_ctx()->hwctx) : nullptr;
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

        int frames_ctx_init() { return av_hwframe_ctx_init(frames_ctx_); }

        [[nodiscard]] AVBufferRef *frames_ctx_ref() const { return av_buffer_ref(frames_ctx_); }

        [[nodiscard]] AVBufferRef *frames_ctx_buf() const { return frames_ctx_; }

        [[nodiscard]] AVHWFramesContext *frames_ctx() const
        {
            return frames_ctx_ ? reinterpret_cast<AVHWFramesContext *>(frames_ctx_->data) : nullptr;
        }

    private:
        AVBufferRef *device_ctx_{};
        AVBufferRef *frames_ctx_{};
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