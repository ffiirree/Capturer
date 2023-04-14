#ifndef CAPTURER_HWACCEL_H
#define CAPTURER_HWACCEL_H

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/hwcontext.h>
#include <libavfilter/avfilter.h>
#include <libavcodec/avcodec.h>
}
#include <string>
#include <vector>
#include <memory>

namespace hwaccel 
{
    struct device_t 
    {
        std::string name{};
        enum AVHWDeviceType type { AV_HWDEVICE_TYPE_NONE };
        AVBufferRef* ref{ nullptr };

        device_t(std::string n, AVHWDeviceType t, AVBufferRef* r)
            : name(std::move(n)), type(t), ref(r) { }

        device_t(const device_t&) = delete;
        device_t& operator=(const device_t&) = delete;

        device_t(device_t&&) = delete;
        device_t& operator=(device_t&&) = delete;

        ~device_t()
        {
            type = AV_HWDEVICE_TYPE_NONE;
            if (!ref) av_buffer_unref(&ref);
        }
    };

    inline bool is_support(enum AVHWDeviceType type)
    {
        return av_hwdevice_find_type_by_name(av_hwdevice_get_type_name(type)) != AV_HWDEVICE_TYPE_NONE;
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

    inline std::unique_ptr<device_t> create_device(AVHWDeviceType hwtype)
    {
        AVBufferRef* device_ref = nullptr;

        if (av_hwdevice_ctx_create(&device_ref, hwtype, nullptr, nullptr, 0) < 0) {
            return nullptr;
        };

        return std::make_unique<device_t>(av_hwdevice_get_type_name(hwtype), hwtype, device_ref);
    }

    inline std::unique_ptr<device_t> create_device(const std::string& name)
    {
        auto hw_type = av_hwdevice_find_type_by_name(name.c_str());
        if (hw_type == AV_HWDEVICE_TYPE_NONE) {
            return nullptr;
        }

        return create_device(hw_type);
    }

    inline int setup_for_filter_graph(AVFilterGraph* graph, const std::unique_ptr<device_t>& device)
    {
        if (!graph || device->type == AV_HWDEVICE_TYPE_NONE || !device->ref) {
            return -1;
        }

        for (unsigned i = 0; i < graph->nb_filters; ++i) {
            graph->filters[i]->hw_device_ctx = av_buffer_ref(device->ref);
        }

        return 0;
    }

    inline int setup_for_encoding(AVCodecContext* ctx, const std::unique_ptr<device_t>& device)
    {
        if (!ctx || device->type == AV_HWDEVICE_TYPE_NONE || !device->ref) {
            return -1;
        }
        ctx->hw_device_ctx = av_buffer_ref(device->ref);
        return 0;
    }
}

#endif //!CAPTURER_HWACCEL_H