#ifndef CAPTURER_HWACCEL_H
#define CAPTURER_HWACCEL_H

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/hwcontext.h>
#include <libavfilter/avfilter.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
}
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace hwaccel 
{
    struct hwdevice_t 
    {
        std::string name{};
        enum AVHWDeviceType type { AV_HWDEVICE_TYPE_NONE };
        AVBufferRef* ref{ nullptr };
        AVBufferRef* frames_ctx{ nullptr };

        hwdevice_t(std::string n, AVHWDeviceType t, AVBufferRef* r)
            : name(std::move(n)), type(t), ref(r) { }

        hwdevice_t(const hwdevice_t&) = delete;
        hwdevice_t& operator=(const hwdevice_t&) = delete;

        hwdevice_t(hwdevice_t&&) = delete;
        hwdevice_t& operator=(hwdevice_t&&) = delete;

        ~hwdevice_t()
        {
            type = AV_HWDEVICE_TYPE_NONE;
            if (!ref) av_buffer_unref(&ref);
        }
    };

    inline std::vector<std::shared_ptr<hwdevice_t>> hwdevices;       // global

    inline bool is_support(enum AVHWDeviceType type)
    {
        for (auto t = av_hwdevice_iterate_types(AV_HWDEVICE_TYPE_NONE);
            t != AV_HWDEVICE_TYPE_NONE;
            t = av_hwdevice_iterate_types(t)) {
            if (t == type) {
                return true;
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

    inline std::shared_ptr<hwdevice_t> find_or_create_device(AVHWDeviceType hwtype)
    {
        for (auto& dev : hwdevices) {
            if (dev->type == hwtype) {
                return dev;
            }
        }

        AVBufferRef* device_ref = nullptr;

        if (av_hwdevice_ctx_create(&device_ref, hwtype, nullptr, nullptr, 0) < 0) {
            return nullptr;
        };

        return hwdevices.emplace_back(
            std::make_shared<hwdevice_t>(
                av_hwdevice_get_type_name(hwtype),
                hwtype,
                device_ref
            )
        );
    }

    inline std::shared_ptr<hwdevice_t> find_or_create_device(const std::string& name)
    {
        auto hw_type = av_hwdevice_find_type_by_name(name.c_str());
        if (hw_type == AV_HWDEVICE_TYPE_NONE) {
            return nullptr;
        }

        return find_or_create_device(hw_type);
    }

    inline int setup_for_filter_graph(AVFilterGraph* graph, AVHWDeviceType type)
    {
        if (!graph || !is_support(type)) {
            return -1;
        }

        auto dev = find_or_create_device(type);
        if (!dev) {
            return -1;
        }

        for (unsigned i = 0; i < graph->nb_filters; ++i) {
            graph->filters[i]->hw_device_ctx = av_buffer_ref(dev->ref);
        }

        return 0;
    }

    inline int set_sink_frame_ctx_for_encoding(AVFilterContext* filter, AVHWDeviceType type)
    {
        if (!filter || !filter->hw_device_ctx) {
            return -1;
        }

        for (auto& dev : hwdevices) {
            if (dev->type == type) {
                dev->frames_ctx = av_buffersink_get_hw_frames_ctx(filter);
                return 0;
            }
        }
        return -1;
    }

    inline int setup_for_encoding(AVCodecContext* ctx, AVHWDeviceType type)
    {
        if (!ctx || !is_support(type)) {
            return -1;
        }

        auto dev = find_or_create_device(type);
        if (!dev || !dev->ref || !dev->frames_ctx) {
            return -1;
        }

        ctx->hw_frames_ctx = av_buffer_ref(dev->frames_ctx);
        ctx->hw_device_ctx = av_buffer_ref(dev->ref);
        return 0;
    }
}

#endif //!CAPTURER_HWACCEL_H