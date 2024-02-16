#include "texture-widget-d3d11.h"

#ifdef Q_OS_WIN

#include "ConvertPixelShader.h"
#include "ConvertVertexShader.h"
#include "libcap/hwaccel.h"
#include "logging.h"

#include <dxgi1_3.h>
#include <fmt/format.h>
#include <probe/util.h>

extern "C" {
#include <libavutil/hwcontext_d3d11va.h>
}

// clang-format off
// triangle strip: {0, 1, 2}, {1, 3, 2}
static constexpr float vertices[] = {
    -1.0f, -1.0f, 0.0f, /* bottom left  */ 0.0, 1.0,
    -1.0f, +1.0f, 0.0f, /* top    left  */ 0.0, 0.0,
    +1.0f, -1.0f, 0.0f, /* bottom right */ 1.0, 1.0,
    +1.0f, +1.0f, 0.0f, /* top    right */ 1.0, 0.0
};
// clang-format on

static constexpr FLOAT black[4]{ 0.0, 0.0, 0.0, 1.0 };

TextureD3D11Widget::TextureD3D11Widget(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::WheelFocus);

    setAttribute(Qt::WA_TransparentForMouseEvents);

    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_NoSystemBackground);

    connect(
        this, &TextureD3D11Widget::arrived, this, [this] { update(); }, Qt::QueuedConnection);
}

TextureD3D11Widget::~TextureD3D11Widget() { device_.detach(); }

std::vector<AVPixelFormat> TextureD3D11Widget::pix_fmts() const
{
    // clang-format off
    return {
        AV_PIX_FMT_D3D11
    };
    // clang-format on
}

bool TextureD3D11Widget::isSupported(AVPixelFormat pix_fmt) const
{
    for (const auto& fmt : pix_fmts()) {
        if (fmt == pix_fmt) return true;
    }
    return false;
}

int TextureD3D11Widget::setFormat(const av::vformat_t& vfmt)
{
    if (!isSupported(vfmt.pix_fmt)) return -1;

    format_       = vfmt;
    config_dirty_ = true;

    return 0;
}

AVRational TextureD3D11Widget::SAR() const
{
    if (format_.sample_aspect_ratio.num > 0 && format_.sample_aspect_ratio.den > 0)
        return format_.sample_aspect_ratio;

    return { 1, 1 };
}

AVRational TextureD3D11Widget::DAR() const
{
    if (!format_.width || !format_.height) return { 0, 0 };

    auto       sar = SAR();
    AVRational dar{};
    av_reduce(&dar.num, &dar.den, static_cast<int64_t>(format_.width) * sar.num,
              static_cast<int64_t>(format_.height) * sar.den, 1024 * 1024);

    return dar;
}

void TextureD3D11Widget::showEvent(QShowEvent *event)
{
    if (!context_) initializeD3D11();

    QWidget::showEvent(event);
}

void TextureD3D11Widget::paintEvent(QPaintEvent *) { paintD3D11(); }

void TextureD3D11Widget::resizeEvent(QResizeEvent *event)
{
    if (context_) resizeD3D11(width(), height());

    QWidget::resizeEvent(event);
}

void TextureD3D11Widget::initializeD3D11()
{
    // 0. DXGI Swap Chain
    auto hwctx = av::hwaccel::find_context(AV_HWDEVICE_TYPE_D3D11VA);
    winrt::check_bool(hwctx);

    device_.attach(hwctx->native_device<AVD3D11VADeviceContext, ID3D11Device>()); // do not take ownership
    device_->GetImmediateContext(context_.put());

    DXGI_SWAP_CHAIN_DESC swap_desc{
        .BufferDesc =
            DXGI_MODE_DESC{
                .Width            = static_cast<UINT>(width()),
                .Height           = static_cast<UINT>(height()),
                .RefreshRate      = { 0, 0 },
                .Format           = DXGI_FORMAT_R8G8B8A8_UNORM,
                .ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
                .Scaling          = DXGI_MODE_SCALING_STRETCHED,
            },
        .SampleDesc   = { .Count = 1, .Quality = 0 },
        .BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount  = 2,
        .OutputWindow = reinterpret_cast<HWND>(winId()),
        .Windowed     = TRUE,
        .SwapEffect   = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
    };
    winrt::com_ptr<::IDXGIAdapter> adapter{};
    winrt::check_hresult(device_.as<IDXGIDevice2>()->GetAdapter(adapter.put()));
    winrt::com_ptr<::IDXGIFactory2> factory{};
    factory.capture(adapter, &IDXGIAdapter::GetParent);
    winrt::check_hresult(factory->CreateSwapChain(device_.get(), &swap_desc, swap_chain_.put()));

    // 1. Input-Assembler State
    // 1.1 vertex buffer
    D3D11_BUFFER_DESC vbd{
        .ByteWidth           = sizeof(vertices),
        .BindFlags           = D3D11_BIND_VERTEX_BUFFER,
        .StructureByteStride = sizeof(float) * 5,
    };
    D3D11_SUBRESOURCE_DATA vsd{ .pSysMem = vertices };
    winrt::check_hresult(device_->CreateBuffer(&vbd, &vsd, vertex_buffer_.put()));

    ID3D11Buffer *vertex_buffers[] = { vertex_buffer_.get() };
    UINT          strides[1]       = { sizeof(float) * 5 };
    UINT          offsets[1]       = { 0 };
    context_->IASetVertexBuffers(0, 1, vertex_buffers, strides, offsets);

    // 1.2 index buffer : optional

    // 1.3 input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    winrt::check_hresult(device_->CreateInputLayout(layout, static_cast<UINT>(std::size(layout)), g_vs_main,
                                                    sizeof(g_vs_main), vertex_layout_.put()));
    context_->IASetInputLayout(vertex_layout_.get());

    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP); // specify the primitive type

    // 2. Vertex Shader Stage
    proj_ = DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f);

    D3D11_BUFFER_DESC cbd = {
        .ByteWidth      = sizeof(proj_),
        .Usage          = D3D11_USAGE_DYNAMIC,
        .BindFlags      = D3D11_BIND_CONSTANT_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };
    D3D11_SUBRESOURCE_DATA csd = { .pSysMem = &proj_ };
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
    UpdateViewport(width(), height());

    // 7. Pixel Shader Stage
    CreateTextureAndShaderResourceViews();

    ID3D11ShaderResourceView *srvs[] = { srv0_.get(), srv1_.get() };
    context_->PSSetShaderResources(0, static_cast<UINT>(std::size(srvs)), srvs);

    D3D11_SAMPLER_DESC sampler_desc{
        .Filter        = D3D11_FILTER_ANISOTROPIC,
        .AddressU      = D3D11_TEXTURE_ADDRESS_WRAP,
        .AddressV      = D3D11_TEXTURE_ADDRESS_WRAP,
        .AddressW      = D3D11_TEXTURE_ADDRESS_WRAP,
        .MaxAnisotropy = 16,
    };
    device_->CreateSamplerState(&sampler_desc, sampler_.put());
    ID3D11SamplerState *samplers[] = { sampler_.get() };
    context_->PSSetSamplers(0, 1, samplers);

    winrt::check_hresult(
        device_->CreatePixelShader(g_ps_main, sizeof(g_ps_main), nullptr, pixel_shader_.put()));
    context_->PSSetShader(pixel_shader_.get(), 0, 0);

    // 8. Output-Merger Stage
    SetupRenderTargets();
}

void TextureD3D11Widget::UpdateViewport(int w, int h)
{
    viewport_ = {
        .TopLeftX = 0,
        .TopLeftY = 0,
        .Width    = static_cast<FLOAT>(w),
        .Height   = static_cast<FLOAT>(h),
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f,
    };
    context_->RSSetViewports(1, &viewport_);
}

void TextureD3D11Widget::CreateTextureAndShaderResourceViews()
{
    D3D11_TEXTURE2D_DESC td{
        .Width          = static_cast<UINT>(width()),
        .Height         = static_cast<UINT>(height()),
        .MipLevels      = 1,
        .ArraySize      = 1,
        .Format         = DXGI_FORMAT_NV12,
        .SampleDesc     = { 1, 0 },
        .Usage          = D3D11_USAGE_DEFAULT,
        .BindFlags      = D3D11_BIND_SHADER_RESOURCE,
        .CPUAccessFlags = 0,
        .MiscFlags      = D3D11_RESOURCE_MISC_SHARED,
    };
    device_->CreateTexture2D(&td, nullptr, texture_.put());

    // Y
    D3D11_SHADER_RESOURCE_VIEW_DESC srvd0 = CD3D11_SHADER_RESOURCE_VIEW_DESC{
        texture_.get(),
        D3D11_SRV_DIMENSION_TEXTURE2D,
        DXGI_FORMAT_R8_UNORM,
    };
    device_->CreateShaderResourceView(texture_.get(), &srvd0, srv0_.put());

    // UV
    D3D11_SHADER_RESOURCE_VIEW_DESC srvd1 = CD3D11_SHADER_RESOURCE_VIEW_DESC{
        texture_.get(),
        D3D11_SRV_DIMENSION_TEXTURE2D,
        DXGI_FORMAT_R8G8_UNORM,
    };
    device_->CreateShaderResourceView(texture_.get(), &srvd1, srv1_.put());
}

void TextureD3D11Widget::SetupRenderTargets()
{
    context_->OMSetRenderTargets(0, nullptr, nullptr);
    rtv_ = {};

    winrt::com_ptr<ID3D11Texture2D> bbuffer{};
    winrt::check_hresult(swap_chain_->GetBuffer(0, winrt::guid_of<ID3D11Texture2D>(), bbuffer.put_void()));

    CD3D11_RENDER_TARGET_VIEW_DESC rtvd(D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM);
    winrt::check_hresult(device_->CreateRenderTargetView(bbuffer.get(), &rtvd, rtv_.put()));

    ID3D11RenderTargetView *rtvs[] = { rtv_.get() };
    context_->OMSetRenderTargets(1, rtvs, nullptr);
}

void TextureD3D11Widget::resizeD3D11(int w, int h)
{
    UpdateViewport(w, h);

    //
    SetupRenderTargets();

    // keep aspect ratio
    proj_ = DirectX::XMMatrixScaling(1, 1, 1);
    if (auto dar = DAR(); dar.num && dar.den) {
        auto size = QSizeF(dar.num, dar.den).scaled(w, h, Qt::KeepAspectRatio);
        proj_     = DirectX::XMMatrixScaling(size.width() / w, size.height() / h, 1);
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    context_->Map(proj_buffer_.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &proj_, sizeof(proj_));
    context_->Unmap(proj_buffer_.get(), 0);

    ID3D11Buffer *cbs[] = { proj_buffer_.get() };
    context_->VSSetConstantBuffers(0, 1, cbs);

    repaint();
}

void TextureD3D11Widget::paintD3D11()
{
    context_->ClearRenderTargetView(rtv_.get(), black);

    ID3D11RenderTargetView *rtvs[] = { rtv_.get() };
    context_->OMSetRenderTargets(1, rtvs, nullptr);

    {
        std::lock_guard lock(mtx_);
        context_->CopySubresourceRegion(texture_.get(), 0, 0, 0, 0,
                                        reinterpret_cast<::ID3D11Texture2D *>(frame_->data[0]),
                                        reinterpret_cast<intptr_t>(frame_->data[1]), 0);
    }

    context_->Draw(4, 0);

    swap_chain_->Present(1, 0);
}

void TextureD3D11Widget::present(const av::frame& frame)
{
    if (!frame || !frame->data[0] || frame->width <= 0 || frame->height <= 0 ||
        frame->format == AV_PIX_FMT_NONE) {
        LOG(WARNING) << "invalid frame.";
        return;
    }

    {
        std::lock_guard lock(mtx_);
        frame_ = frame;

        if ((format_.width != frame_->width || format_.height != frame_->height) ||
            (format_.pix_fmt != frame_->format) ||
            (format_.color.space != frame_->colorspace || format_.color.range != frame_->color_range)) {

            format_.width               = frame_->width;
            format_.height              = frame_->height;
            format_.pix_fmt             = static_cast<AVPixelFormat>(frame_->format);
            format_.sample_aspect_ratio = frame_->sample_aspect_ratio;

            format_.color = {
                frame_->colorspace,
                frame_->color_range,
                frame_->color_primaries,
                frame_->color_trc,
            };

            config_dirty_ = true;
        }
    }

    emit arrived();
}

#endif