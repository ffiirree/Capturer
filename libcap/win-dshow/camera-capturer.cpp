#include "libcap/win-dshow/camera-capturer.h"

#ifdef _WIN32

#include "libcap/platform.h"
#include "logging.h"

int DShowCameraCapturer::open(const std::string&, std::map<std::string, std::string>)
{
    RETURN_NEGV_IF_FAILED(::CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER,
                                             IID_IGraphBuilder, graph_.put_void()));

    RETURN_NEGV_IF_FAILED(::CoCreateInstance(CLSID_CaptureGraphBuilder2, nullptr, CLSCTX_INPROC_SERVER,
                                             IID_ICaptureGraphBuilder2, builder_.put_void()));

    RETURN_NEGV_IF_FAILED(builder_->SetFiltergraph(graph_.get()));

    RETURN_NEGV_IF_FAILED(graph_->QueryInterface(IID_IMediaControl, control_.put_void()));

    RETURN_NEGV_IF_FAILED(graph_->AddFilter(filter_.get(), nullptr));

    // builder_->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, filter_.get(), IID_IAMStreamConfig);
    return 0;
}

void DShowCameraCapturer::enable(const AVMediaType t, const bool value)
{
    if (t == AVMEDIA_TYPE_VIDEO) enabled_ = value;
}

int DShowCameraCapturer::start() { return 0; }

bool DShowCameraCapturer::has(const AVMediaType mt) const { return mt == AVMEDIA_TYPE_VIDEO; }

#endif