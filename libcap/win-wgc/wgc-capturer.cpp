#ifdef _WIN32

#include "libcap/win-wgc/wgc-capturer.h"

#include "fmt/format.h"
#include "fmt/ranges.h"
#include "logging.h"
#include "probe/defer.h"

#include <regex>

extern "C" {
#include <libavutil/hwcontext_d3d11va.h>
}

using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

void WindowsGraphicsCapturer::parse_options(std::map<std::string, std::string>& options)
{
    // options
    if (options.contains("draw_mouse")) {
        draw_mouse_ =
            std::regex_match(options["draw_mouse"], std::regex("1|on|true", std::regex_constants::icase));
    }

    if (options.contains("show_region")) {
        show_region_ =
            std::regex_match(options["show_region"], std::regex("1|on|true", std::regex_constants::icase));
    }

    // capture box
    if (options.contains("offset_x") && options.contains("offset_y") && options.contains("video_size") &&
        std::regex_match(options.at("offset_x"), std::regex("\\d+")) &&
        std::regex_match(options.at("offset_y"), std::regex("\\d+"))) {
        std::smatch matchs;
        if (std::regex_match(options.at("video_size"), matchs, std::regex("([\\d]+)x([\\d]+)"))) {
            box_.left   = std::stoul(options.at("offset_x"));
            box_.top    = std::stoul(options.at("offset_y"));
            // the box region includes the left pixel but not the right pixel.
            box_.right  = box_.left + std::stoul(matchs[1]);
            box_.bottom = box_.top + std::stoul(matchs[2]);
        }
    }
}

int WindowsGraphicsCapturer::open(const std::string& name, std::map<std::string, std::string> options)
{
    std::lock_guard lock(mtx_);

    LOG(INFO) << fmt::format("[     WGC] [{}] options = {}", name, options);

    parse_options(options);

    // parse capture item
    std::smatch matches;
    if (std::regex_match(name, matches, std::regex("window=(\\d+)"))) {
        item_ = wgc::create_capture_item_for_window(reinterpret_cast<HWND>(std::stoull(matches[1])));
    }
    else if (std::regex_match(name, matches, std::regex("display=(\\d+)"))) {
        item_ = wgc::create_capture_item_for_monitor(reinterpret_cast<HMONITOR>(std::stoull(matches[1])));
    }

    if (!item_) {
        LOG(ERROR) << "[     WGC] can not create capture item for: '" << name << "'.";
        return -1;
    }
    closed_ = item_.Closed(winrt::auto_revoke, { this, &WindowsGraphicsCapturer::on_closed });

    // IDirect3DDevice: FFmpeg -> D3D11 -> DXGI -> WinRT @{
    auto ffmpeg_d3d11_device = hwaccel::find_or_create_device(AV_HWDEVICE_TYPE_D3D11VA);
    if (!ffmpeg_d3d11_device) {
        LOG(ERROR) << "[     WGC] can not create the FFmpeg d3d11device.";
        return -1;
    }

    // D3D11Device & ImmediateContext
    AVHWDeviceContext *device_ctx = reinterpret_cast<AVHWDeviceContext *>(ffmpeg_d3d11_device->ptr()->data);
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
    // winrt d3d11device
    device_ = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
    // @}

    // create framepool
    frame_pool_ = Direct3D11CaptureFramePool::CreateFreeThreaded(
        device_, DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, item_.Size());

    // callback
    frame_arrived_ =
        frame_pool_.FrameArrived(winrt::auto_revoke, { this, &WindowsGraphicsCapturer::on_frame_arrived });

    session_ = frame_pool_.CreateCaptureSession(item_);

    // cursor & border
    if (wgc::is_cursor_toggle_supported()) session_.IsCursorCaptureEnabled(draw_mouse_);
    if (wgc::is_border_toggle_supported()) session_.IsBorderRequired(show_region_);

    // must be multiple of 2, round downwards
    if (box_ == D3D11_BOX{ .front = 0, .back = 1 }) {
        box_.right  = item_.Size().Width;
        box_.bottom = item_.Size().Height;
    }

    // TODO: Window Mode: change this
    box_.right  = box_.left + ((std::min<UINT>(box_.right, item_.Size().Width) - box_.left) & ~0x01);
    box_.bottom = box_.top + ((std::min<UINT>(box_.bottom, item_.Size().Height) - box_.top) & ~0x01);

    if (box_.right <= box_.left || box_.bottom <= box_.top) {
        LOG(ERROR) << fmt::format("[     WGC] [{}] invalid recording region <({},{}), ({},{})> in ({}x{})",
                                  name, box_.left, box_.top, box_.right, box_.bottom, item_.Size().Width,
                                  item_.Size().Height);
        return -1;
    }

    // video output format
    vfmt = av::vformat_t{
        .width               = static_cast<int>(box_.right - box_.left),
        .height              = static_cast<int>(box_.bottom - box_.top),
        .pix_fmt             = AV_PIX_FMT_D3D11,
        .framerate           = { 60, 1 },
        .sample_aspect_ratio = { 1, 1 },
        .time_base           = { 1, OS_TIME_BASE },
        .hwaccel             = AV_HWDEVICE_TYPE_D3D11VA,
    };

    // TODO: Window Capture Mode: resize the texture2d
    D3D11_TEXTURE2D_DESC desc{
        .Width          = static_cast<UINT>(vfmt.width),
        .Height         = static_cast<UINT>(vfmt.height),
        .MipLevels      = 1,
        .ArraySize      = 1,
        .Format         = DXGI_FORMAT_B8G8R8A8_UNORM,
        .SampleDesc     = { 1, 0 },
        .Usage          = D3D11_USAGE_DEFAULT,
        .BindFlags      = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
        .CPUAccessFlags = 0,
        .MiscFlags      = 0,
    };
    if (FAILED(d3d11_device->CreateTexture2D(&desc, 0, resize_texture_.put()))) {
        LOG(INFO) << "failed to create render texture.";
        return -1;
    }

    D3D11_RENDER_TARGET_VIEW_DESC target_desc{
        .Format        = DXGI_FORMAT_B8G8R8A8_UNORM,
        .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
        .Texture2D     = { .MipSlice = 0 },
    };

    d3d11_device->CreateRenderTargetView(reinterpret_cast<ID3D11Resource *>(resize_texture_.get()),
                                         &target_desc, resize_render_target_.put());

    D3D11_SHADER_RESOURCE_VIEW_DESC view_desc{
        .Format        = DXGI_FORMAT_B8G8R8A8_UNORM,
        .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
        .Texture2D     = { .MostDetailedMip = 0, .MipLevels = 1 },
    };

    d3d11_device->CreateShaderResourceView(reinterpret_cast<ID3D11Resource *>(resize_texture_.get()),
                                           &view_desc, resize_shader_view_.put());

    // FFmpeg hardware frame pool
    if (init_hwframes_ctx() < 0) return -1;

    eof_   = 0x00;
    ready_ = true;

    return 0;
}

int WindowsGraphicsCapturer::run()
{
    std::lock_guard lock(mtx_);

    if (!ready_ || running_) {
        LOG(ERROR) << "[     WGC] not ready or already running.";
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

std::vector<av::vformat_t> WindowsGraphicsCapturer::vformats() const
{
    return {
        {
            .pix_fmt = AV_PIX_FMT_D3D11,
            .hwaccel = AV_HWDEVICE_TYPE_D3D11VA,
        },
        {
            .pix_fmt = AV_PIX_FMT_BGRA,
            .hwaccel = AV_HWDEVICE_TYPE_NONE,
        },
    };
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

void WindowsGraphicsCapturer::on_frame_arrived(const Direct3D11CaptureFramePool& sender,
                                               const winrt::Windows::Foundation::IInspectable&)
{
    std::lock_guard lock(mtx_);

    if (!running_) return;

    auto frame         = sender.TryGetNextFrame();
    auto frame_surface = wgc::get_interface_from<::ID3D11Texture2D>(frame.Surface());

    D3D11_TEXTURE2D_DESC desc{};
    frame_surface->GetDesc(&desc);

    // TODO: Window Capture Mode: resize the frame pool
    // if (static_cast<int>(desc.Width) != framepool_size.width || static_cast<int>(desc.Height) !=
    // framepool_size.height) {
    //     frame_pool_.Recreate(device_, DirectXPixelFormat::B8G8R8A8UIntNormalized, 2,
    //                          { static_cast<int32_t>(desc.Width), static_cast<int32_t>(desc.Height) });
    // }

    // allocate texture for hardware frame
    if (av_hwframe_get_buffer(reinterpret_cast<AVBufferRef *>(frames_ref_), frame_.put(), 0) < 0) {
        LOG(ERROR) << "av_hwframe_get_buffer";
        return;
    }

    frame_->sample_aspect_ratio = { 1, 1 };
    frame_->pts                 = os_gettime_ns();

    // copy the texture to the FFmpeg frame
    // 1. Display   Mode: CopyResource
    // 2. Window    Mode: CopyResource and reisze the texture2d [TODO]
    // 3. Rectangle Mode: Rectangle region of Display, CopySubresourceRegion
    if (vfmt.height != static_cast<int>(desc.Height) || vfmt.width != static_cast<int>(desc.Width)) {
        d3d11_ctx_->CopySubresourceRegion(reinterpret_cast<::ID3D11Texture2D *>(frame_->data[0]),
                                          static_cast<UINT>(reinterpret_cast<intptr_t>(frame_->data[1])), 0,
                                          0, 0, frame_surface.get(), 0, &box_);
    }
    else {
        d3d11_ctx_->CopyResource(reinterpret_cast<::ID3D11Texture2D *>(frame_->data[0]),
                                 frame_surface.get());
    }

    // transfer frame to CPU if needed
    hwaccel::transfer_frame(frame_.get(), vfmt.pix_fmt);

    DLOG(INFO) << fmt::format("[V]  frame = {:>5d}, pts = {:>14d}, size = {:>4d}x{:>4d}", frame_number_++,
                              frame_->pts, frame_->width, frame_->height);

    // push FFmpeg frame to our frame pool
    buffer_.push([this](AVFrame *frame) {
        av_frame_unref(frame);
        av_frame_move_ref(frame, frame_.get());
    });
}

void WindowsGraphicsCapturer::on_closed(const GraphicsCaptureItem&,
                                        const winrt::Windows::Foundation::IInspectable&)
{
    // TODO: Window Capture Mode
    LOG(INFO) << "OnClosed";
}

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
        box_          = { .front = 0, .back = 1 };

        buffer_.clear();

        eof_ = 0x01;

        LOG(INFO) << "[       WGC] RESET";
    }
}

#endif