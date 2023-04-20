//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH 
// THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//*********************************************************

#include <Unknwn.h>
#include <inspectable.h>
#include "wgc-capturer.h"
#include "logging.h"
#include "fmt/format.h"
#include "defer.h"

using namespace winrt;
using namespace Windows;
using namespace Windows::Foundation;
using namespace Windows::System;
using namespace Windows::Graphics;
using namespace Windows::Graphics::Capture;
using namespace Windows::Graphics::DirectX;
using namespace Windows::Graphics::DirectX::Direct3D11;
using namespace Windows::Foundation::Numerics;
using namespace Windows::UI;
using namespace Windows::UI::Composition;

static auto create_d3ddevice()
{
    auto d3d_device = CreateD3DDevice();
    auto dxgi_device = d3d_device.as<IDXGIDevice>();

    winrt::com_ptr<::IInspectable> inspectable;
    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi_device.get(), inspectable.put()));
    
    return inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
}

const AVPixelFormat HWACCEL_OUTPUT_FMT = AV_PIX_FMT_BGRA;

int WgcCapturer::open(HWND window, HMONITOR monitor)
{
    if (window) {
        item_ = GraphicsCaptureItem::TryCreateFromWindowId(WindowId(reinterpret_cast<uint64_t>(window)));
    }
    else if (monitor) {
        item_ = GraphicsCaptureItem::TryCreateFromDisplayId(DisplayId(reinterpret_cast<uint64_t>(monitor)));
    }
    
    if (!item_) {
        LOG(ERROR) << "[     WGC] can not create GraphicsCaptureItem.";
        return -1;
    }

    device_ = create_d3ddevice();

    // Set up 
    auto d3d11device = GetDXGIInterfaceFromObject<ID3D11Device>(device_);
    d3d11device->GetImmediateContext(d3d11_ctx_.put());

    // Create framepool, define pixel format (DXGI_FORMAT_B8G8R8A8_UNORM), and frame size. 
    // m_framePool = Direct3D11CaptureFramePool::Create(
    frame_pool_ = Direct3D11CaptureFramePool::CreateFreeThreaded(
        device_,
        DirectXPixelFormat::B8G8R8A8UIntNormalized,
        2,
        item_.Size()
    );
    frame_arrived_ = frame_pool_.FrameArrived(auto_revoke, { this, &WgcCapturer::OnFrameArrived });
    item_.Closed(winrt::auto_revoke, { this, &WgcCapturer::OnClosed });
    session_ = frame_pool_.CreateCaptureSession(item_);

    last_size_ = item_.Size();
    vfmt = vformat_t{
        last_size_.Width,
        last_size_.Height,
        HWACCEL_OUTPUT_FMT,
        { 60, 1 },
        { 1, 1 },
        { 1, OS_TIME_BASE },
        AV_HWDEVICE_TYPE_D3D11VA
    };

    frame_ = av_frame_alloc();
    if (!frame_) {
        return -1;
    }

    if (init_hwframes_ctx() < 0) {
        return -1;
    }

    ready_ = true;

    return 0;
}

int WgcCapturer::run()
{
    if (!ready_ || running_) {
        LOG(ERROR) << "[     WGC] not ready or running.";
        return -1;
    }

    running_ = true;
    session_.StartCapture();
    return 0;
}

int WgcCapturer::produce(AVFrame* frame, int type)
{
    if (type != AVMEDIA_TYPE_VIDEO) {
        LOG(ERROR) << "[       WGC] unsupported media type : " << av_get_media_type_string(static_cast<AVMediaType>(type));
        return -1;
    }

    if (buffer_.empty()) return (eof_ & 0x01) ? AVERROR_EOF : AVERROR(EAGAIN);

    buffer_.pop([frame](AVFrame* popped) {
        av_frame_unref(frame);
        av_frame_move_ref(frame, popped);
    });
    return 0;
}

bool WgcCapturer::empty(int)
{
    return buffer_.empty();
}

bool WgcCapturer::has(int mt) const
{
    return mt == AVMEDIA_TYPE_VIDEO;
}

std::string WgcCapturer::format_str(int mt) const
{
    if (mt != AVMEDIA_TYPE_VIDEO) {
        LOG(ERROR) << "not supported";
        return {};
    }

    return fmt::format(
        "video_size={}x{}:pix_fmt={}:time_base={}/{}:pixel_aspect={}/{}:frame_rate={}/{}",
        vfmt.width, vfmt.height, vfmt.pix_fmt,
        vfmt.time_base.num, vfmt.time_base.den,
        vfmt.sample_aspect_ratio.num, vfmt.sample_aspect_ratio.den,
        vfmt.framerate.num, vfmt.framerate.den
    );
}

AVRational WgcCapturer::time_base(int) const
{
    return vfmt.time_base;
}

// Process captured frames
void WgcCapturer::reset()
{
    auto expected = true;
    if (running_.compare_exchange_strong(expected, false))
    {
		frame_arrived_.revoke();
        closed_.revoke();
		frame_pool_.Close();
        session_.Close();

        av_frame_free(&frame_);
        av_buffer_unref(&frames_ref_);
        av_buffer_unref(&device_ref_);

        //swap_chain_ = nullptr;
        frame_pool_ = nullptr;
        session_ = nullptr;
        d3d11_ctx_ = nullptr;
        device_ = nullptr;
        item_ = nullptr;

        frame_number_ = 0;

        LOG(INFO) << "[       WGC] RESET";
    }
}

int WgcCapturer::init_hwframes_ctx()
{
    auto dev = hwaccel::find_or_create_device(AV_HWDEVICE_TYPE_D3D11VA);
    if (!dev) {
        LOG(ERROR) << "[      WGC] can not create hardware device for " << av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_D3D11VA);
        return -1;
    }

    device_ref_ = dev->ref();

    //
    frames_ref_ = av_hwframe_ctx_alloc(device_ref_);
    if (!frames_ref_) {
        LOG(ERROR) << "[      WGC] av_hwframe_ctx_alloc";
        return -1;
    }

    dev->frames_ctx_ref(frames_ref_);

    frames_ctx_ = reinterpret_cast<AVHWFramesContext*>(frames_ref_->data);

    frames_ctx_->format = AV_PIX_FMT_D3D11;
    frames_ctx_->height = last_size_.Height;
    frames_ctx_->width = last_size_.Width;
    frames_ctx_->sw_format = AV_PIX_FMT_BGRA;

    if (av_hwframe_ctx_init(frames_ref_) < 0) {
        LOG(ERROR) << "[      WGC] av_hwframe_ctx_init";
        return -1;
    }

    return 0;
}

void WgcCapturer::OnFrameArrived(Direct3D11CaptureFramePool const& sender, winrt::Windows::Foundation::IInspectable const&)
{
    auto frame = sender.TryGetNextFrame();
    // TODO: obs-studio says: the ContentSize is not reliable
    auto frame_size = frame.ContentSize();
    auto frame_surface = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());

    D3D11_TEXTURE2D_DESC desc{};
    frame_surface->GetDesc(&desc);

    if (frame_size.Width != last_size_.Width || frame_size.Height != last_size_.Height)
    {
        frame_pool_.Recreate(
            device_,
            DirectXPixelFormat::B8G8R8A8UIntNormalized,
            2,
            frame_size
        );

        last_size_ = frame_size;
        vfmt.height = last_size_.Height;
        vfmt.width = last_size_.Width;
    }
    av_frame_unref(frame_);
    if (av_hwframe_get_buffer(reinterpret_cast<AVBufferRef*>(frames_ref_), frame_, 0) < 0) {
        LOG(ERROR) << "av_hwframe_get_buffer";
        return;
    }

    frame_->sample_aspect_ratio = { 1, 1 };
    frame_->pts = os_gettime_ns();

    d3d11_ctx_->CopyResource(reinterpret_cast<ID3D11Texture2D*>(frame_->data[0]), frame_surface.get());

    DLOG(INFO) << fmt::format("[V]  frame = {:>5d}, pts = {:>14d}", frame_number_++, frame_->pts);

    hwaccel::transfer_frame(frame_, vfmt.pix_fmt);

    // av_frame_unref(frame_);
    buffer_.push([this](AVFrame* frame) {
       av_frame_unref(frame);
       av_frame_move_ref(frame, frame_);
    });
}

void WgcCapturer::OnClosed(
    GraphicsCaptureItem const&,
    winrt::Windows::Foundation::IInspectable const&)
{
    LOG(INFO) << "OnClosed";
}