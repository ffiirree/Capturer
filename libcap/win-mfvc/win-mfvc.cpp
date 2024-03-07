#include "libcap/win-mfvc/win-mfvc.h"

#ifdef _WIN32

#include "logging.h"
#include "probe/util.h"

#include <mfidl.h>
#include <probe/defer.h>
#include <winrt/base.h>

namespace mfvc
{
    std::vector<av::device_t> video_devices()
    {
        std::vector<av::device_t> list{};

        try {
            winrt::com_ptr<IMFAttributes> attributes{};
            winrt::check_hresult(MFCreateAttributes(attributes.put(), 1));
            winrt::check_hresult(attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                                                     MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID));

            UINT32        count{};
            IMFActivate **activate{};
            winrt::check_hresult(MFEnumDeviceSources(attributes.get(), &activate, &count));
            defer(for (UINT32 i = 0; i < count; ++i) { activate[i]->Release(); } CoTaskMemFree(activate));
            if (count <= 0) return {};

            winrt::com_ptr<IMFMediaSource> camera{};
            for (UINT32 i = 0; i < count; ++i) {

                winrt::check_hresult(activate[i]->ActivateObject(IID_PPV_ARGS(camera.put())));

                WCHAR *FriendlyName{};
                UINT32 NameLength{};
                winrt::check_hresult(activate[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
                                                                     &FriendlyName, &NameLength));
                defer(CoTaskMemFree(FriendlyName));

                WCHAR *Link{};
                UINT32 LinkLength;
                winrt::check_hresult(activate[i]->GetAllocatedString(
                    MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &Link, &LinkLength));
                defer(CoTaskMemFree(Link));

                list.push_back({
                    .name   = probe::util::to_utf8(FriendlyName, NameLength),
                    .id     = probe::util::to_utf8(Link, LinkLength),
                    .type   = av::device_type_t::video | av::device_type_t::source,
                    .format = av::device_format_t::MediaFoundation,
                });
            }
        }
        catch (const winrt::hresult_error& e) {
            loge("[     WIN-MF] {}", probe::util::to_utf8(e.message().c_str()));
            return {};
        }

        return list;
    }

    std::pair<AVCodecID, AVPixelFormat> to_ffmpeg_format(const GUID& fmt)
    {
        switch (fmt.Data1) {
        case 20 /*MFVideoFormat_RGB24*/:         return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_RGB24 };
        case 21 /*MFVideoFormat_ARGB32*/:        return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_ARGB };
        case 22 /*MFVideoFormat_RGB32*/:         return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_0RGB };
        case 23 /*MFVideoFormat_RGB565*/:        return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_RGB565 };
        case 24 /*MFVideoFormat_RGB555*/:        return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_RGB555 };
        case 41 /*MFVideoFormat_RGB8*/:          return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_RGB8 };
        case 50 /*MFVideoFormat_L8*/:            return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_GRAY8 };
        case 80 /*MFVideoFormat_D16*/:
        case 81 /*MFVideoFormat_L16*/:           return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_GRAY16 };
        case FCC('NV12') /*MFVideoFormat_NV12*/: return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_NV12 };
        case FCC('NV21') /*MFVideoFormat_NV21*/: return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_NV21 };
        case FCC('YVU9') /*MFVideoFormat_YVU9*/: return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_YUV410P };
        case FCC('YV12') /*MFVideoFormat_YV12*/:
        case FCC('IYUV') /*MFVideoFormat_IYUV*/:
        case FCC('I420') /*MFVideoFormat_I420*/: return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_YUV420P };
        case FCC('YUY2') /*MFVideoFormat_YUY2*/: return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_YUYV422 };
        case FCC('YVYU') /*MFVideoFormat_YVYU*/: return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_YVYU422 };
        case FCC('UYVY') /*MFVideoFormat_UYVY*/: return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_UYVY422 };
        case FCC('AYUV') /*MFVideoFormat_AYUV*/: return { AV_CODEC_ID_RAWVIDEO, AV_PIX_FMT_VUYA };
        case FCC('MJPG') /*MFVideoFormat_MJPG*/: return { AV_CODEC_ID_MJPEG, AV_PIX_FMT_NONE };
        case FCC('H263') /*MFVideoFormat_H263*/: return { AV_CODEC_ID_H263, AV_PIX_FMT_NONE };
        case FCC('H264') /*MFVideoFormat_H264*/: return { AV_CODEC_ID_H264, AV_PIX_FMT_NONE };
        case FCC('H265') /*MFVideoFormat_H265*/: return { AV_CODEC_ID_H265, AV_PIX_FMT_NONE };
        case FCC('VP80') /*MFVideoFormat_VP80*/: return { AV_CODEC_ID_VP8, AV_PIX_FMT_NONE };
        case FCC('VP90') /*MFVideoFormat_VP90*/: return { AV_CODEC_ID_VP9, AV_PIX_FMT_NONE };
        case FCC('HEVC') /*MFVideoFormat_HEVC*/: return { AV_CODEC_ID_HEVC, AV_PIX_FMT_NONE };
        default:                                 return { AV_CODEC_ID_NONE, AV_PIX_FMT_NONE };
        }
    }
} // namespace mfvc

#endif