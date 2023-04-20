#ifndef WGC_CAPTURER_H
#define WGC_CAPTURER_H

#include "win-wgc.h"
extern "C" {
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include <libavutil/hwcontext_d3d11va.h>
}
#include "producer.h"
#include "ringvector.h"
#include "hwaccel.h"

class WgcCapturer : public Producer<AVFrame>
{
public:
    ~WgcCapturer() { reset(); }

    int open(HWND wid, HMONITOR monitor = nullptr);

    void reset() override;

    int run() override;

    int produce(AVFrame*, int)override;
    bool empty(int) override;

    bool has(int mt) const override;
    std::string format_str(int) const override;
    AVRational time_base(int) const override;

private:
    void OnFrameArrived(
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
        winrt::Windows::Foundation::IInspectable const& args);

    void OnClosed(
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem const&,
        winrt::Windows::Foundation::IInspectable const&);

    int init_hwframes_ctx();

private:
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item_{ nullptr };
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice device_{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession session_{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool frame_pool_{ nullptr };

    winrt::Windows::Graphics::SizeInt32 last_size_{ };

    winrt::com_ptr<ID3D11DeviceContext> d3d11_ctx_{ nullptr };

    winrt::Windows::Graphics::Capture::GraphicsCaptureItem::Closed_revoker closed_;
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::FrameArrived_revoker frame_arrived_;

    AVBufferRef* device_ref_{ nullptr };

    AVBufferRef* frames_ref_{ nullptr };
    AVHWFramesContext* frames_ctx_{ nullptr };

    AVFrame* frame_{ nullptr };
    uint32_t frame_number_{};

    RingVector<AVFrame*, 8> buffer_{
        []() { return av_frame_alloc(); },
        [](AVFrame** frame) { av_frame_free(frame); }
    };
};

#endif // !WGC_CAPTURER_H