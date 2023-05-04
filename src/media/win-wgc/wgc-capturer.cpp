#ifdef _WIN32

#include "wgc-capturer.h"

#include "defer.h"
#include "fmt/format.h"
#include "fmt/ranges.h"
#include "logging.h"
#include "probe/graphics.h"

#include <regex>

using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

int WindowsGraphicsCapturer::open(const std::string& name, std::map<std::string, std::string> options)
{
    std::lock_guard lock(mtx_);

    LOG(INFO) << fmt::format("[     WGC] [{}] options = {}", name, options);

    // options
    if (options.contains("draw_mouse")) {
        draw_mouse_ = (options["draw_mouse"] == "1" || options["draw_mouse"] == "true");
        options.erase("draw_mouse");
    }

    if (options.contains("show_region")) {
        show_region_ = (options["show_region"] == "1" || options["show_region"] == "true");
        options.erase("show_region");
    }

    // parse capture item
    std::smatch matches;
    if (std::regex_match(name, matches, std::regex("window=(\\d+)"))) {
        if (matches.size() == 2) {
            item_ =
                wgc::create_capture_item_for_window(reinterpret_cast<HWND>(std::stoull(matches[1].str())));

            // framerate
            // auto display = probe::graphics::display_contains(
            //    reinterpret_cast<uint64_t>(std::stoull(matches[1].str())));
            // if (display.has_value() && display->frequency > 20) {
            //    vfmt.framerate = { std::lround(display->frequency), 1 };
            //}
        }
    }
    else if (std::regex_match(name, matches, std::regex("display=(\\d+)"))) {
        if (matches.size() == 2) {
            item_ = wgc::create_capture_item_for_monitor(
                reinterpret_cast<HMONITOR>(std::stoull(matches[1].str())));
        }
    }

    if (!item_) {
        LOG(ERROR) << "[     WGC] can not create GraphicsCaptureItem.";
        return -1;
    }

    auto hwdevice = hwaccel::find_or_create_device(AV_HWDEVICE_TYPE_D3D11VA);
    if (hwdevice == nullptr) {
        LOG(ERROR) << "[     WGC] can not create d3d11device.";
        return -1;
    }

    // D3D11Device & ImmediateContext
    // winrt::com_ptr<::ID3D11Device> d3d11_device = wgc::create_d3d11device();
    AVHWDeviceContext *device_ctx = reinterpret_cast<AVHWDeviceContext *>(hwdevice->ptr()->data);
    ID3D11Device *d3d11_device    = reinterpret_cast<AVD3D11VADeviceContext *>(device_ctx->hwctx)->device;
    d3d11_device->GetImmediateContext(d3d11_ctx_.put());

    // If you're using C++/WinRT, then to move back and forth between IDirect3DDevice and IDXGIDevice, use
    // the IDirect3DDxgiInterfaceAccess::GetInterface and CreateDirect3D11DeviceFromDXGIDevice functions.
    //
    // This represents an IDXGIDevice, and can be used to interop between Windows Runtime components that
    // need to exchange IDXGIDevice references.
    winrt::com_ptr<::IInspectable> inspectable{};
    winrt::com_ptr<IDXGIDevice> dxgi_device{};
    d3d11_device->QueryInterface(winrt::guid_of<IDXGIDevice>(), dxgi_device.put_void());
    if (FAILED(::CreateDirect3D11DeviceFromDXGIDevice(dxgi_device.get(), inspectable.put()))) {
        LOG(ERROR) << "[     WGC] CreateDirect3D11DeviceFromDXGIDevice failed.";
        return -1;
    }
    device_ = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();

    // Create framepool
    frame_pool_ = Direct3D11CaptureFramePool::CreateFreeThreaded(
        device_, DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, item_.Size());

    frame_arrived_ =
        frame_pool_.FrameArrived(winrt::auto_revoke, { this, &WindowsGraphicsCapturer::on_frame_arrived });

    item_.Closed(winrt::auto_revoke, { this, &WindowsGraphicsCapturer::on_closed });

    session_ = frame_pool_.CreateCaptureSession(item_);
    session_.IsCursorCaptureEnabled(draw_mouse_);
    session_.IsBorderRequired(show_region_);

    // video format
    vfmt = av::vformat_t{
        .width               = item_.Size().Width,
        .height              = item_.Size().Height,
        .pix_fmt             = AV_PIX_FMT_D3D11,
        .framerate           = { 60, 1 },
        .sample_aspect_ratio = { 1, 1 },
        .time_base           = { 1, OS_TIME_BASE },
        .hwaccel             = AV_HWDEVICE_TYPE_D3D11VA,
    };

    if (init_hwframes_ctx() < 0) {
        return -1;
    }

    ready_ = true;
    eof_   = 0x00;

    return 0;
}

int WindowsGraphicsCapturer::run()
{
    std::lock_guard lock(mtx_);

    if (!ready_ || running_) {
        LOG(ERROR) << "[     WGC] not ready or running.";
        return -1;
    }

    running_ = true;
    session_.StartCapture();
    return 0;
}

int WindowsGraphicsCapturer::produce(AVFrame *frame, int type)
{
    if (type != AVMEDIA_TYPE_VIDEO) {
        LOG(ERROR) << "[       WGC] unsupported media type : "
                   << av::to_string(static_cast<AVMediaType>(type));
        return -1;
    }

    if (buffer_.empty()) return (eof_ & 0x01) ? AVERROR_EOF : AVERROR(EAGAIN);

    buffer_.pop([frame](AVFrame *popped) {
        av_frame_unref(frame);
        av_frame_move_ref(frame, popped);
    });
    return 0;
}

bool WindowsGraphicsCapturer::empty(int) { return buffer_.empty(); }

bool WindowsGraphicsCapturer::has(int mt) const { return mt == AVMEDIA_TYPE_VIDEO; }

std::string WindowsGraphicsCapturer::format_str(int mt) const
{
    if (mt != AVMEDIA_TYPE_VIDEO) {
        LOG(ERROR) << "not supported";
        return {};
    }

    return av::to_string(vfmt);
}

AVRational WindowsGraphicsCapturer::time_base(int) const { return vfmt.time_base; }

// Process captured frames
void WindowsGraphicsCapturer::reset()
{
    std::lock_guard lock(mtx_);

    auto expected = true;
    if (running_.compare_exchange_strong(expected, false)) {
        frame_arrived_.revoke();
        closed_.revoke();
        frame_pool_.Close();
        session_.Close();

        // TODO: ERROR! the thread may not exit yet
        av_buffer_unref(&frames_ref_);
        av_buffer_unref(&device_ref_);

        frame_pool_ = nullptr;
        session_    = nullptr;
        d3d11_ctx_  = nullptr;
        device_     = nullptr;
        item_       = nullptr;

        frame_number_ = 0;

        buffer_.clear();

        eof_ = 0x01;

        LOG(INFO) << "[       WGC] RESET";
    }
}

int WindowsGraphicsCapturer::init_hwframes_ctx()
{
    auto dev = hwaccel::find_or_create_device(AV_HWDEVICE_TYPE_D3D11VA);
    if (!dev) {
        LOG(ERROR) << "[      WGC] can not create hardware device for "
                   << av::to_string(AV_HWDEVICE_TYPE_D3D11VA);
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

    frames_ctx_ = reinterpret_cast<AVHWFramesContext *>(frames_ref_->data);

    frames_ctx_->format    = AV_PIX_FMT_D3D11;
    frames_ctx_->height    = vfmt.height;
    frames_ctx_->width     = vfmt.width;
    frames_ctx_->sw_format = AV_PIX_FMT_BGRA;

    if (av_hwframe_ctx_init(frames_ref_) < 0) {
        LOG(ERROR) << "[      WGC] av_hwframe_ctx_init";
        return -1;
    }

    return 0;
}

void WindowsGraphicsCapturer::on_frame_arrived(Direct3D11CaptureFramePool const& sender,
                                               winrt::Windows::Foundation::IInspectable const&)
{
    std::lock_guard lock(mtx_);

    auto frame         = sender.TryGetNextFrame();
    auto frame_surface = wgc::get_interface_from<::ID3D11Texture2D>(frame.Surface());

    D3D11_TEXTURE2D_DESC desc{};
    frame_surface->GetDesc(&desc);

    if (static_cast<int>(desc.Width) != vfmt.width || static_cast<int>(desc.Height) != vfmt.height) {
        frame_pool_.Recreate(device_, DirectXPixelFormat::B8G8R8A8UIntNormalized, 2,
                             { static_cast<int32_t>(desc.Width), static_cast<int32_t>(desc.Height) });

        vfmt.height = desc.Height;
        vfmt.width  = desc.Width;
    }

    if (av_hwframe_get_buffer(reinterpret_cast<AVBufferRef *>(frames_ref_), frame_.put(), 0) < 0) {
        LOG(ERROR) << "av_hwframe_get_buffer";
        return;
    }

    frame_->sample_aspect_ratio = { 1, 1 };
    frame_->pts                 = os_gettime_ns();

    d3d11_ctx_->CopyResource(reinterpret_cast<::ID3D11Texture2D *>(frame_->data[0]), frame_surface.get());

    DLOG(INFO) << fmt::format("[V]  frame = {:>5d}, pts = {:>14d}, size = {:>4d}x{:>4d}", frame_number_++,
                              frame_->pts, frame_->width, frame_->height);

    hwaccel::transfer_frame(frame_.get(), vfmt.pix_fmt);

    buffer_.push([this](AVFrame *frame) {
        av_frame_unref(frame);
        av_frame_move_ref(frame, frame_.get());
    });
}

void WindowsGraphicsCapturer::on_closed(GraphicsCaptureItem const&,
                                        winrt::Windows::Foundation::IInspectable const&)
{
    LOG(INFO) << "OnClosed";
}

#endif