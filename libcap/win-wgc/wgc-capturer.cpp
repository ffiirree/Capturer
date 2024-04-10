#ifdef _WIN32

#include "libcap/win-wgc/wgc-capturer.h"

#include "logging.h"
#include "ResizingPixelShader.h"
#include "ResizingVertexShader.h"

#include <fmt/chrono.h>

extern "C" {
#include <libavutil/hwcontext_d3d11va.h>
}

namespace winrt
{
    using namespace winrt::Windows::Foundation;
    using namespace winrt::Windows::Graphics::Capture;
    using namespace winrt::Windows::Graphics::DirectX;
    using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
} // namespace winrt

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

int WindowsGraphicsCapturer::open(const std::string&, std::map<std::string, std::string>)
{
    try {
        // @{ IDirect3DDevice: FFmpeg -> D3D11 -> DXGI -> WinRT
        hwctx_ = av::hwaccel::get_context(AV_HWDEVICE_TYPE_D3D11VA);
        winrt::check_pointer(hwctx_.get());

        // D3D11Device & ImmediateContext
        context_.copy_from(hwctx_->native_context<AVD3D11VADeviceContext>());
        device_.copy_from(hwctx_->native_device<AVD3D11VADeviceContext>());

        winrt_device_ = wgc::CreateDirect3DDevice(device_);
        // @}

        switch (level) {
        case CAPTURE_DISPLAY: item_ = wgc::CreateCaptureItem(reinterpret_cast<HMONITOR>(handle)); break;
        case CAPTURE_WINDOW:  item_ = wgc::CreateCaptureItem(reinterpret_cast<HWND>(handle)); break;
        case CAPTURE_DESKTOP:
        default:              loge("[        WGC] unsupported"); return av::UNSUPPORTED;
        }

        onclosed_ = item_.Closed(winrt::auto_revoke, { this, &WindowsGraphicsCapturer::OnClosed });

        // create framepool
        size_       = item_.Size();
        frame_pool_ = winrt::Direct3D11CaptureFramePool::CreateFreeThreaded(
            winrt_device_, winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, size_);

        // callback
        onarrived_ = frame_pool_.FrameArrived(winrt::auto_revoke,
                                              { this, &WindowsGraphicsCapturer::OnFrameArrived });

        session_ = frame_pool_.CreateCaptureSession(item_);

        // cursor & border
        if (wgc::IsCursoToggleSupported()) session_.IsCursorCaptureEnabled(draw_cursor);
        if (wgc::IsBorderToggleSupported()) session_.IsBorderRequired(show_region);
    }
    catch (const winrt::hresult_error& e) {
        loge("[        WGC] {}", probe::util::to_utf8(e.message().c_str()));
        return -1;
    }

    //
    left = box_.left = std::max<int>(0, left);
    top = box_.top = std::max<int>(0, top);

    vfmt.width  = vfmt.width <= 0 ? item_.Size().Width : vfmt.width;
    vfmt.height = vfmt.height <= 0 ? item_.Size().Height : vfmt.height;

    box_.right  = left + ((std::min<int32_t>(left + vfmt.width + 1, item_.Size().Width) - left) & ~1);
    box_.bottom = top + ((std::min<int32_t>(top + vfmt.height + 1, item_.Size().Height) - top) & ~1);

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

    if (level == CAPTURE_WINDOW && InitalizeResizingResources() < 0) return -1;

    eof_   = 0x00;
    ready_ = true;

    return 0;
}

int WindowsGraphicsCapturer::start()
{
    if (!ready_ || running_) {
        loge("[       WGC] not ready or already running.");
        return -1;
    }

    running_ = true;
    session_.StartCapture();
    return 0;
}

std::vector<av::vformat_t> WindowsGraphicsCapturer::video_formats() const
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

    ID3D11Buffer  *vertex_buffers[] = { vertex_buffer_.get() };
    constexpr UINT strides[1]       = { sizeof(float) * 5 };
    constexpr UINT offsets[1]       = { 0 };
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
    if (FAILED(device_->CreateTexture2D(&target_desc, nullptr, target_.put()))) {
        logi("[       WGC] failed to create render target texture.");
        return -1;
    }

    constexpr D3D11_RENDER_TARGET_VIEW_DESC rtv_desc{
        .Format        = DXGI_FORMAT_B8G8R8A8_UNORM,
        .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
        .Texture2D     = { .MipSlice = 0 },
    };

    if (FAILED(device_->CreateRenderTargetView(reinterpret_cast<ID3D11Resource *>(target_.get()), &rtv_desc,
                                               rtv_.put()))) {
        logi("[       WGC] failed to create render target view.");
        return -1;
    }

    ID3D11RenderTargetView *rtvs[] = { rtv_.get() };
    context_->OMSetRenderTargets(1, rtvs, nullptr);

    return 0;
}

int WindowsGraphicsCapturer::InitializeHWFramesContext()
{
    if (hwctx_->frames_ctx_alloc(); !hwctx_->frames_ctx) return av::NOMEM;

    const auto frames_ctx = hwctx_->frames_ctx.data();

    frames_ctx->format    = AV_PIX_FMT_D3D11;
    frames_ctx->height    = vfmt.height;
    frames_ctx->width     = vfmt.width;
    frames_ctx->sw_format = AV_PIX_FMT_BGRA;

    if (hwctx_->frames_ctx_init() < 0) {
        loge("[       WGC] av_hwframe_ctx_init");
        return -1;
    }

    return 0;
}

void WindowsGraphicsCapturer::OnFrameArrived(const winrt::Direct3D11CaptureFramePool& sender,
                                             const winrt::IInspectable&)
{
    std::lock_guard lock(mtx_);

    if (!running_) return;

    const auto d3d11frame    = sender.TryGetNextFrame();
    const auto frame_texture = wgc::GetInterfaceFrom<::ID3D11Texture2D>(d3d11frame.Surface());

    // surface size
    D3D11_TEXTURE2D_DESC tex_desc{};
    frame_texture->GetDesc(&tex_desc);

    // Window Capture Mode: resize the frame pool to frame size
    if (d3d11frame.ContentSize() != size_) {
        size_ = d3d11frame.ContentSize();
        frame_pool_.Recreate(winrt_device_, winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2,
                             d3d11frame.ContentSize());
    }

    av::frame frame{};
    // allocate texture for hardware frame
    if (av_hwframe_get_buffer(hwctx_->frames_ctx.get(), frame.put(), 0) < 0) {
        loge("[       WGC] failed to alloc a hardware frame.");
        return;
    }

    frame->sample_aspect_ratio = { 1, 1 };
    frame->pts                 = av::clock::ns().count();
    // According to MSDN, all integer formats contain sRGB image data
    frame->color_range         = vfmt.color.range;
    frame->color_primaries     = vfmt.color.primaries;
    frame->color_trc           = vfmt.color.transfer;
    frame->colorspace          = vfmt.color.space;

    // copy the texture to the FFmpeg frame
    // 1. Display   Mode: CopyResource
    // 2. Window    Mode: CopyResource and reisze the texture2d [TODO]
    // 3. Rectangle Mode: Rectangle region of Display, CopySubresourceRegion
    switch (level) {
    case CAPTURE_WINDOW:
        if (vfmt.height != static_cast<int>(tex_desc.Height) ||
            vfmt.width != static_cast<int>(tex_desc.Width)) {
            context_->ClearRenderTargetView(rtv_.get(), black);

            proj_ = DirectX::XMMatrixScaling(1, 1, 1);

            const double r1      = tex_desc.Width / static_cast<double>(tex_desc.Height);
            const double r2      = vfmt.width / static_cast<double>(vfmt.height);
            const float  scale_x = (r1 > r2) ? 1.0f : static_cast<float>(vfmt.height * r1 / vfmt.width);
            const float  scale_y = (r1 > r2) ? static_cast<float>(vfmt.width / (r1 * vfmt.height)) : 1.0f;

            proj_ = DirectX::XMMatrixScaling(scale_x, scale_y, 1);

            D3D11_MAPPED_SUBRESOURCE mapped{};
            winrt::check_hresult(context_->Map(proj_buffer_.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
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
            winrt::check_hresult(device_->CreateTexture2D(&source_desc, nullptr, source_.put()));

            const CD3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{
                source_.get(),
                D3D11_SRV_DIMENSION_TEXTURE2D,
                DXGI_FORMAT_B8G8R8A8_UNORM,
                tex_desc.MipLevels - 1,
                tex_desc.MipLevels,
            };
            winrt::check_hresult(device_->CreateShaderResourceView(source_.get(), &srv_desc, srv_.put()));

            ID3D11ShaderResourceView *srvs[] = { srv_.get() };
            context_->PSSetShaderResources(0, static_cast<UINT>(std::size(srvs)), srvs);

            ID3D11RenderTargetView *rtvs[] = { rtv_.get() };
            context_->OMSetRenderTargets(1, rtvs, nullptr);

            context_->CopyResource(source_.get(), frame_texture.get());
            context_->Draw(4, 0);
            context_->CopyResource(reinterpret_cast<::ID3D11Texture2D *>(frame->data[0]), target_.get());
        }
        else {
            context_->CopyResource(reinterpret_cast<::ID3D11Texture2D *>(frame->data[0]),
                                   frame_texture.get());
        }
        break;
    case CAPTURE_DISPLAY:
        // Rectangle
        if (vfmt.height != static_cast<int>(tex_desc.Height) ||
            vfmt.width != static_cast<int>(tex_desc.Width)) {
            context_->CopySubresourceRegion(reinterpret_cast<::ID3D11Texture2D *>(frame->data[0]),
                                            static_cast<UINT>(reinterpret_cast<intptr_t>(frame->data[1])),
                                            0, 0, 0, frame_texture.get(), 0, &box_);
        }
        // Monitor
        else {
            context_->CopyResource(reinterpret_cast<::ID3D11Texture2D *>(frame->data[0]),
                                   frame_texture.get());
        }
        break;
    }

    // transfer frame to CPU if needed
    av::hwaccel::transfer_frame(frame.get(), vfmt.pix_fmt);

    logd("[V] size = {:>4d}x{:>4d}, ts = {:.3%T}", frame->width, frame->height,
         std::chrono::nanoseconds{ frame->pts });

    onarrived(frame, AVMEDIA_TYPE_VIDEO);
}

void WindowsGraphicsCapturer::OnClosed(const winrt::GraphicsCaptureItem&, const winrt::IInspectable&)
{
    // the captured window is closed
    eof_ = 0x01;

    logw("[       WGC] capture item is closed.");
}

// Process captured frames
void WindowsGraphicsCapturer::stop()
{
    std::lock_guard lock(mtx_);

    running_ = false;

    onarrived_.revoke();
    onclosed_.revoke();

    if (frame_pool_) {
        frame_pool_.Close();
        frame_pool_ = nullptr;
    }

    if (session_) {
        session_.Close();
        session_ = nullptr;
    }
}

WindowsGraphicsCapturer::~WindowsGraphicsCapturer()
{
    stop();

    logi("[       WGC] ~");
}

#endif