#ifdef _WIN32

#include "libcap/win-wgc/wgc-capturer.h"

#include "fmt/format.h"
#include "fmt/ranges.h"
#include "logging.h"
#include "ResizingPixelShader.h"
#include "ResizingVertexShader.h"

#include <regex>

extern "C" {
#include <libavutil/hwcontext_d3d11va.h>
}

using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

// clang-format off
// triangle strip: {0, 1, 2}, {1, 3, 2}
static constexpr float vertices[] = {
    -1.0f, -1.0f, 0.0f, /* bottom left  */ 0.0, 1.0,
    -1.0f, +1.0f, 0.0f, /* top    left  */ 0.0, 0.0,
    +1.0f, -1.0f, 0.0f, /* bottom right */ 1.0, 1.0,
    +1.0f, +1.0f, 0.0f, /* top    right */ 1.0, 0.0
};
// clang-format on

static constexpr FLOAT black[4]{ 0.0, 0.0, 0.0, 0.0 };

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
    if (std::smatch matches; std::regex_match(name, matches, std::regex("window=(\\d+)"))) {
        item_ = wgc::create_capture_item_for_window(reinterpret_cast<HWND>(std::stoull(matches[1])));
        mode_ = mode_t::window;
    }
    else if (std::regex_match(name, matches, std::regex("display=(\\d+)"))) {
        item_ = wgc::create_capture_item_for_monitor(reinterpret_cast<HMONITOR>(std::stoull(matches[1])));
        mode_ = mode_t::monitor;
    }

    if (!item_) {
        LOG(ERROR) << "[     WGC] can not create capture item for: '" << name << "'.";
        return -1;
    }
    onclosed_ = item_.Closed(winrt::auto_revoke, { this, &WindowsGraphicsCapturer::OnClosed });

    // @{ IDirect3DDevice: FFmpeg -> D3D11 -> DXGI -> WinRT
    hwctx_ = av::hwaccel::get_context(AV_HWDEVICE_TYPE_D3D11VA);
    if (!hwctx_) {
        LOG(ERROR) << "[     WGC] can not create the FFmpeg d3d11device.";
        return -1;
    }

    // D3D11Device & ImmediateContext
    device_.attach(hwctx_->native_device<AVD3D11VADeviceContext, ID3D11Device>()); // do not take ownership
    device_->GetImmediateContext(context_.put());

    // If you're using C++/WinRT, then to move back and forth between IDirect3DDevice and IDXGIDevice, use
    // the IDirect3DDxgiInterfaceAccess::GetInterface and CreateDirect3D11DeviceFromDXGIDevice functions.
    //
    // This represents an IDXGIDevice, and can be used to interop between Windows Runtime components that
    // need to exchange IDXGIDevice references.
    winrt::com_ptr<::IInspectable> inspectable{};
    const auto dxgi_device = device_.as<IDXGIDevice>();
    if (FAILED(::CreateDirect3D11DeviceFromDXGIDevice(dxgi_device.get(), inspectable.put()))) {
        LOG(ERROR) << "[       WGC] CreateDirect3D11DeviceFromDXGIDevice failed.";
        return -1;
    }
    // winrt d3d11device
    winrt_device_ = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
    // @}

    // create framepool
    frame_pool_ = Direct3D11CaptureFramePool::CreateFreeThreaded(
        winrt_device_, DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, item_.Size());

    // callback
    onarrived_ =
        frame_pool_.FrameArrived(winrt::auto_revoke, { this, &WindowsGraphicsCapturer::OnFrameArrived });

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
        LOG(ERROR) << fmt::format(
            "[       WGC] [{}] invalid recording region <({},{}), ({},{})> in ({}x{})", name, box_.left,
            box_.top, box_.right, box_.bottom, item_.Size().Width, item_.Size().Height);
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
        .color               = { AVCOL_SPC_RGB, AVCOL_RANGE_JPEG, AVCOL_PRI_BT709, AVCOL_TRC_IEC61966_2_1 },
        .hwaccel             = AV_HWDEVICE_TYPE_D3D11VA,
        .sw_pix_fmt          = AV_PIX_FMT_BGRA,
    };

    // FFmpeg hardware frame pool
    if (InitializeHWFramesContext() < 0) return -1;

    if (mode_ == mode_t::window && InitalizeResizingResources() < 0) return -1;

    eof_   = 0x00;
    ready_ = true;

    return 0;
}

int WindowsGraphicsCapturer::run()
{
    std::lock_guard lock(mtx_);

    if (!ready_ || running_) {
        LOG(ERROR) << "[       WGC] not ready or already running.";
        return -1;
    }

    running_ = true;
    session_.StartCapture();
    return 0;
}

int WindowsGraphicsCapturer::produce(AVFrame *frame, AVMediaType type)
{
    if (type != AVMEDIA_TYPE_VIDEO) {
        LOG(ERROR) << "[       WGC] unsupported media type : " << av::to_string(type);
        return -1;
    }

    if (buffer_.empty()) return (eof_ & 0x01) ? AVERROR_EOF : AVERROR(EAGAIN);

    buffer_.pop([frame](AVFrame *popped) {
        av_frame_unref(frame);
        av_frame_move_ref(frame, popped);
    });
    return 0;
}

bool WindowsGraphicsCapturer::empty(AVMediaType) { return buffer_.empty(); }

bool WindowsGraphicsCapturer::has(AVMediaType mt) const { return mt == AVMEDIA_TYPE_VIDEO; }

std::string WindowsGraphicsCapturer::format_str(AVMediaType mt) const
{
    if (mt != AVMEDIA_TYPE_VIDEO) {
        LOG(ERROR) << "[       WGC] not supported";
        return {};
    }

    return av::to_string(vfmt);
}

AVRational WindowsGraphicsCapturer::time_base(AVMediaType) const { return vfmt.time_base; }

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

int WindowsGraphicsCapturer::InitalizeResizingResources()
{
    // 1. Input-Assembler State
    // 1.1 vertex buffer
    constexpr D3D11_BUFFER_DESC vbd{
        .ByteWidth           = sizeof(vertices),
        .BindFlags           = D3D11_BIND_VERTEX_BUFFER,
        .StructureByteStride = sizeof(float) * 5,
    };
    constexpr D3D11_SUBRESOURCE_DATA vsd{ .pSysMem = vertices };
    winrt::check_hresult(device_->CreateBuffer(&vbd, &vsd, vertex_buffer_.put()));

    ID3D11Buffer *vertex_buffers[]       = { vertex_buffer_.get() };
    constexpr UINT strides[1]            = { sizeof(float) * 5 };
    constexpr UINT offsets[1]                = { 0 };
    context_->IASetVertexBuffers(0, 1, vertex_buffers, strides, offsets);

    // 1.2 index buffer : optional

    // 1.3 input layout
    constexpr D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    winrt::check_hresult(device_->CreateInputLayout(layout, static_cast<UINT>(std::size(layout)), g_vs_main,
                                                    sizeof(g_vs_main), vertex_layout_.put()));
    context_->IASetInputLayout(vertex_layout_.get());

    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // 2. Vertex Shader Stage
    proj_ = DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f);

    constexpr D3D11_BUFFER_DESC cbd = {
        .ByteWidth      = sizeof(proj_),
        .Usage          = D3D11_USAGE_DYNAMIC,
        .BindFlags      = D3D11_BIND_CONSTANT_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };
    const D3D11_SUBRESOURCE_DATA csd = { .pSysMem = &proj_ };
    winrt::check_hresult(device_->CreateBuffer(&cbd, &csd, proj_buffer_.put()));

    ID3D11Buffer *cbs[] = { proj_buffer_.get() };
    context_->VSSetConstantBuffers(0, 1, cbs);

    winrt::check_hresult(
        device_->CreateVertexShader(g_vs_main, sizeof(g_vs_main), nullptr, vertex_shader_.put()));
    context_->VSSetShader(vertex_shader_.get(), 0, 0);

    // 3. Tesselation Stages

    // 4. Geometry Shader Stage

    // 5. Stream-Output Stage

    // 6. Resterizer Stage
    const D3D11_VIEWPORT viewport{
        .TopLeftX = 0,
        .TopLeftY = 0,
        .Width    = static_cast<FLOAT>(vfmt.width),
        .Height   = static_cast<FLOAT>(vfmt.height),
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f,
    };
    context_->RSSetViewports(1, &viewport);

    // 7. Pixel Shader Stage
    // sampler
    constexpr D3D11_SAMPLER_DESC sampler_desc{
        .Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
        .AddressU       = D3D11_TEXTURE_ADDRESS_WRAP,
        .AddressV       = D3D11_TEXTURE_ADDRESS_WRAP,
        .AddressW       = D3D11_TEXTURE_ADDRESS_WRAP,
        .ComparisonFunc = D3D11_COMPARISON_NEVER,
        .MinLOD         = 0,
        .MaxLOD         = D3D11_FLOAT32_MAX,
    };
    device_->CreateSamplerState(&sampler_desc, sampler_.put());
    ID3D11SamplerState *samplers[] = { sampler_.get() };
    context_->PSSetSamplers(0, 1, samplers);

    // pixel shader
    winrt::check_hresult(
        device_->CreatePixelShader(g_ps_main, sizeof(g_ps_main), nullptr, pixel_shader_.put()));
    context_->PSSetShader(pixel_shader_.get(), 0, 0);

    // 8. Output-Merger Stage
    const D3D11_TEXTURE2D_DESC target_desc{
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
    if (FAILED(device_->CreateTexture2D(&target_desc, 0, target_.put()))) {
        LOG(INFO) << "[       WGC] failed to create render target texture.";
        return -1;
    }

    constexpr D3D11_RENDER_TARGET_VIEW_DESC rtv_desc{
        .Format        = DXGI_FORMAT_B8G8R8A8_UNORM,
        .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
        .Texture2D     = { .MipSlice = 0 },
    };

    if (FAILED(device_->CreateRenderTargetView(reinterpret_cast<ID3D11Resource *>(target_.get()), &rtv_desc,
                                               rtv_.put()))) {
        LOG(INFO) << "[       WGC] failed to create render target view.";
        return -1;
    }

    ID3D11RenderTargetView *rtvs[] = { rtv_.get() };
    context_->OMSetRenderTargets(1, rtvs, nullptr);

    return 0;
}

int WindowsGraphicsCapturer::InitializeHWFramesContext()
{
    if (const auto frames_ctx_ref = hwctx_->frames_ctx_alloc(); !frames_ctx_ref) {
        LOG(ERROR) << "[       WGC] av_hwframe_ctx_alloc";
        return -1;
    }

    const auto frames_ctx = hwctx_->frames_ctx();

    frames_ctx->format    = AV_PIX_FMT_D3D11;
    frames_ctx->height    = vfmt.height;
    frames_ctx->width     = vfmt.width;
    frames_ctx->sw_format = AV_PIX_FMT_BGRA;

    if (hwctx_->frames_ctx_init() < 0) {
        LOG(ERROR) << "[       WGC] av_hwframe_ctx_init";
        return -1;
    }

    return 0;
}

void WindowsGraphicsCapturer::OnFrameArrived(const Direct3D11CaptureFramePool& sender,
                                             const winrt::Windows::Foundation::IInspectable&)
{
    std::lock_guard lock(mtx_);

    if (!running_) return;

    const auto frame         = sender.TryGetNextFrame();
    const auto frame_texture = wgc::get_interface_from<::ID3D11Texture2D>(frame.Surface());

    // surface size
    D3D11_TEXTURE2D_DESC tex_desc{};
    frame_texture->GetDesc(&tex_desc);

    // Window Capture Mode: resize the frame pool to frame size
    if (mode_ == mode_t::window && frame.ContentSize() != winrt::Windows::Graphics::SizeInt32{
                                                              static_cast<int32_t>(tex_desc.Height),
                                                              static_cast<int32_t>(tex_desc.Width) }) {
        frame_pool_.Recreate(winrt_device_, DirectXPixelFormat::B8G8R8A8UIntNormalized, 2,
                             frame.ContentSize());
    }

    // allocate texture for hardware frame
    if (av_hwframe_get_buffer(hwctx_->frames_ctx_buf(), frame_.put(), 0) < 0) {
        LOG(ERROR) << "[       WGC] failed to alloc a hardware frame.";
        return;
    }

    frame_->sample_aspect_ratio = { 1, 1 };
    frame_->pts                 = av::clock::ns().count();
    // According to MSDN, all integer formats contain sRGB image data
    frame_->color_range         = AVCOL_RANGE_JPEG;
    frame_->color_primaries     = AVCOL_PRI_BT709;
    frame_->color_trc           = AVCOL_TRC_IEC61966_2_1;
    frame_->colorspace          = AVCOL_SPC_RGB;

    // copy the texture to the FFmpeg frame
    // 1. Display   Mode: CopyResource
    // 2. Window    Mode: CopyResource and reisze the texture2d [TODO]
    // 3. Rectangle Mode: Rectangle region of Display, CopySubresourceRegion
    switch (mode_) {
    case mode_t::window:
        if (vfmt.height != static_cast<int>(tex_desc.Height) ||
            vfmt.width != static_cast<int>(tex_desc.Width)) {
            context_->ClearRenderTargetView(rtv_.get(), black);

            proj_    = DirectX::XMMatrixScaling(1, 1, 1);
            float r1 = tex_desc.Width / static_cast<float>(tex_desc.Height);
            float r2 = vfmt.width / static_cast<float>(vfmt.height);
            auto w   = r1 > r2 ? vfmt.width : vfmt.height * r1;
            auto h   = r1 > r2 ? vfmt.width / r1 : vfmt.height;
            proj_    = DirectX::XMMatrixScaling(w / vfmt.width, h / vfmt.height, 1);

            D3D11_MAPPED_SUBRESOURCE mapped{};
            context_->Map(proj_buffer_.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            memcpy(mapped.pData, &proj_, sizeof(proj_));
            context_->Unmap(proj_buffer_.get(), 0);

            ID3D11Buffer *cbs[] = { proj_buffer_.get() };
            context_->VSSetConstantBuffers(0, 1, cbs);

            const D3D11_TEXTURE2D_DESC source_desc{
                .Width          = tex_desc.Width,
                .Height         = tex_desc.Height,
                .MipLevels      = 1,
                .ArraySize      = 1,
                .Format         = DXGI_FORMAT_B8G8R8A8_UNORM,
                .SampleDesc     = { 1, 0 },
                .Usage          = D3D11_USAGE_DEFAULT,
                .BindFlags      = D3D11_BIND_SHADER_RESOURCE,
                .CPUAccessFlags = 0,
                .MiscFlags      = D3D11_RESOURCE_MISC_SHARED,
            };
            device_->CreateTexture2D(&source_desc, nullptr, source_.put());

            const CD3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{
                source_.get(),
                D3D11_SRV_DIMENSION_TEXTURE2D,
                DXGI_FORMAT_B8G8R8A8_UNORM,
                tex_desc.MipLevels - 1,
                tex_desc.MipLevels,
            };
            device_->CreateShaderResourceView(source_.get(), &srv_desc, srv_.put());

            ID3D11ShaderResourceView *srvs[] = { srv_.get() };
            context_->PSSetShaderResources(0, static_cast<UINT>(std::size(srvs)), srvs);

            ID3D11RenderTargetView *rtvs[] = { rtv_.get() };
            context_->OMSetRenderTargets(1, rtvs, nullptr);

            context_->CopyResource(source_.get(), frame_texture.get());
            context_->Draw(4, 0);
            context_->CopyResource(reinterpret_cast<::ID3D11Texture2D *>(frame_->data[0]), target_.get());
        }
        else {
            context_->CopyResource(reinterpret_cast<::ID3D11Texture2D *>(frame_->data[0]),
                                   frame_texture.get());
        }
        break;
    case mode_t::monitor:
        // Rectangle
        if (vfmt.height != static_cast<int>(tex_desc.Height) ||
            vfmt.width != static_cast<int>(tex_desc.Width)) {
            context_->CopySubresourceRegion(reinterpret_cast<::ID3D11Texture2D *>(frame_->data[0]),
                                            static_cast<UINT>(reinterpret_cast<intptr_t>(frame_->data[1])),
                                            0, 0, 0, frame_texture.get(), 0, &box_);
        }
        // Monitor
        else {
            context_->CopyResource(reinterpret_cast<::ID3D11Texture2D *>(frame_->data[0]),
                                   frame_texture.get());
        }
        break;
    }

    // transfer frame to CPU if needed
    av::hwaccel::transfer_frame(frame_.get(), vfmt.pix_fmt);

    DLOG(INFO) << fmt::format("[V]  frame = {:>5d}, pts = {:>14d}, size = {:>4d}x{:>4d}", frame_number_++,
                              frame_->pts, frame_->width, frame_->height);

    // push FFmpeg frame to our frame pool
    buffer_.push([this](AVFrame *frame) {
        av_frame_unref(frame);
        av_frame_move_ref(frame, frame_.get());
    });
}

void WindowsGraphicsCapturer::OnClosed(const GraphicsCaptureItem&,
                                       const winrt::Windows::Foundation::IInspectable&)
{
    // the captured window is closed
    eof_ = 0x01;

    LOG(INFO) << "[       WGC] window is closed.";
}

// Process captured frames
void WindowsGraphicsCapturer::reset()
{
    std::lock_guard lock(mtx_);

    auto expected = true;
    if (running_.compare_exchange_strong(expected, false)) {
        onarrived_.revoke();
        onclosed_.revoke();
        frame_pool_.Close();
        session_.Close();

        device_.detach();
        context_      = nullptr;
        frame_pool_   = nullptr;
        session_      = nullptr;
        winrt_device_ = nullptr;
        item_         = nullptr;

        frame_number_ = 0;
        box_          = { .front = 0, .back = 1 };

        buffer_.clear();

        eof_ = 0x01;

        // TODO: ERROR! the thread may not exit yet
        hwctx_ = nullptr;

        LOG(INFO) << "[       WGC] RESET";
    }
}

#endif