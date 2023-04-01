#ifndef CAPTURER_MEDIA_H
#define CAPTURER_MEDIA_H

extern "C" {
#include <libavutil/hwcontext.h>
}

inline bool is_support_hwdevice(enum AVHWDeviceType type)
{
    return av_hwdevice_find_type_by_name(av_hwdevice_get_type_name(type)) != AV_HWDEVICE_TYPE_NONE;
}

#endif // !CAPTURER_MEDIA_H