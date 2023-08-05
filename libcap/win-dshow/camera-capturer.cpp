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

    //builder_->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, filter_.get(), IID_IAMStreamConfig);
    return 0;
}

void DShowCameraCapturer::reset() {}

int DShowCameraCapturer::run() { return 0; }

int DShowCameraCapturer::produce(AVFrame *frame, AVMediaType mt)
{
    if (mt != AVMEDIA_TYPE_VIDEO) {
        LOG(ERROR) << "[       WGC] unsupported media type : " << av::to_string(mt);
        return -1;
    }

    std::lock_guard lock(mtx_);

    if (buffer_.empty()) return (eof_ & 0x01) ? AVERROR_EOF : AVERROR(EAGAIN);

    av_frame_unref(frame);
    av_frame_move_ref(frame, buffer_.pop().get());
    return 0;
}

bool DShowCameraCapturer::empty(AVMediaType mt) { return mt != AVMEDIA_TYPE_VIDEO || buffer_.empty(); }

bool DShowCameraCapturer::has(AVMediaType mt) const { return mt == AVMEDIA_TYPE_VIDEO; }

std::string DShowCameraCapturer::format_str(AVMediaType mt) const
{
    if (mt != AVMEDIA_TYPE_VIDEO) {
        LOG(ERROR) << "[     DShow] unsupported media type: " << std::to_string(mt);
        return {};
    }

    return av::to_string(vfmt);
}

AVRational DShowCameraCapturer::time_base(AVMediaType mt) const
{
    if (mt != AVMEDIA_TYPE_VIDEO) {
        LOG(ERROR) << "[     DShow] unsupported media type: " << std::to_string(mt);
        return OS_TIME_BASE_Q;
    }

    return vfmt.time_base;
}
std::vector<av::vformat_t> DShowCameraCapturer::vformats() const { return { vfmt }; }

void DShowCameraCapturer::capture_fn() {}

#endif