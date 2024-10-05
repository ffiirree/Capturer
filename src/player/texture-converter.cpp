#include "texture-converter.h"

#include <rhi/qrhi.h>

#ifdef _WIN32

#include "logging.h"
#include "probe/defer.h"

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
}

static winrt::com_ptr<ID3D11Device1> GetD3DDeviceFromQRhi(QRhi *rhi)
{
    const auto native = reinterpret_cast<const QRhiD3D11NativeHandles *>(rhi->nativeHandles());
    if (!native) return {};

    winrt::com_ptr<ID3D11Device> rhi_device{};
    rhi_device.copy_from(static_cast<ID3D11Device *>(native->dev));

    //
    winrt::com_ptr<ID3D11Device1> dev1{};
    if (!rhi_device.try_as(dev1)) return nullptr;

    return dev1;
}

///

bool TextureBridge::ensure_source(ID3D11Device *dev, ID3D11Texture2D *tex, const QSize& size)
{
    if (is_source_initialized(dev, tex, size)) return true;

    shared_handle.close();

    CD3D11_TEXTURE2D_DESC desc{};
    tex->GetDesc(&desc);

    const UINT w = static_cast<UINT>(size.width());
    const UINT h = static_cast<UINT>(size.height());

    CD3D11_TEXTURE2D_DESC texDesc{ desc.Format, w, h };
    texDesc.MipLevels = 1;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;

    if (dev->CreateTexture2D(&texDesc, nullptr, src_tex.put()) != S_OK) {
        loge("failed to create texture2d");
        return false;
    }

    winrt::com_ptr<IDXGIResource1> res{};
    if (!src_tex.try_as(res)) return false;

    const auto hr =
        res->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ, nullptr, shared_handle.put());

    if (FAILED(hr) || !shared_handle) {
        loge("failed to create shared handle");
        return false;
    }

    if (!src_tex.try_as(src_mtx) || !src_mtx) return false;

    dst_tex = nullptr;
    dst_mtx = nullptr;

    return true;
}

bool TextureBridge::is_source_initialized(ID3D11Device *dev, ID3D11Texture2D *tex, const QSize& size)
{
    if (!src_tex) return false;

    winrt::com_ptr<ID3D11Device> tex_device{};
    src_tex->GetDevice(tex_device.put());

    if (dev != tex_device.get()) return false;

    CD3D11_TEXTURE2D_DESC source_desc{};
    tex->GetDesc(&source_desc);

    CD3D11_TEXTURE2D_DESC current_desc{};
    src_tex->GetDesc(&current_desc);

    if (source_desc.Format != current_desc.Format) return false;

    const UINT width  = static_cast<UINT>(size.width());
    const UINT height = static_cast<UINT>(size.height());

    if (current_desc.Width != width || current_desc.Height != height) return false;

    return true;
}

bool TextureBridge::ensure_sink(const winrt::com_ptr<ID3D11Device1>& dev)
{
    if (dst_device != dev) {
        dst_tex    = nullptr;
        dst_device = dev;
    }

    if (dst_tex) return true;

    if (FAILED(dst_device->OpenSharedResource1(shared_handle.get(), IID_PPV_ARGS(&dst_tex)))) {
        return false;
    }

    CD3D11_TEXTURE2D_DESC desc{};
    dst_tex->GetDesc(&desc);

    desc.MiscFlags = 0;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    if (dst_device->CreateTexture2D(&desc, nullptr, out_tex.put()) != S_OK) return false;

    if (!dst_tex.try_as(dst_mtx)) return false;

    return true;
}

bool TextureBridge::to_shared(ID3D11Device *dev, ID3D11DeviceContext *ctx, ID3D11Texture2D *tex, UINT index,
                              const QSize& size)
{
    if (!ensure_source(dev, tex, size)) return false;

    // flush to ensure that texture is fully updated before we share it.
    ctx->Flush();

    if (FAILED(src_mtx->AcquireSync(SRCKEY, INFINITE))) return false;

    const UINT w = static_cast<UINT>(size.width());
    const UINT h = static_cast<UINT>(size.height());

    const D3D11_BOX crop{ 0, 0, 0, w, h, 1 };
    ctx->CopySubresourceRegion(src_tex.get(), 0, 0, 0, 0, tex, index, &crop);

    src_mtx->ReleaseSync(DSTKEY);

    return true;
}

winrt::com_ptr<ID3D11Texture2D> TextureBridge::from_shared(const winrt::com_ptr<ID3D11Device1>&       dev,
                                                           const winrt::com_ptr<ID3D11DeviceContext>& ctx)
{
    if (!ensure_sink(dev)) return {};

    if (FAILED(dst_mtx->AcquireSync(DSTKEY, INFINITE))) return {};

    ctx->CopySubresourceRegion(out_tex.get(), 0, 0, 0, 0, dst_tex.get(), 0, nullptr);

    dst_mtx->ReleaseSync(SRCKEY);

    return out_tex;
}

///

D3D11TextureConverter::D3D11TextureConverter(QRhi *rhi)
    : TextureConverter(rhi), rhi_device_(GetD3DDeviceFromQRhi(rhi))
{
    if (rhi_device_) rhi_device_->GetImmediateContext(rhi_ctx_.put());
}

uint64_t D3D11TextureConverter::texture(av::frame& frame)
{
    if ((!rhi || rhi->backend() != QRhi::D3D11 || !rhi_device_ || !rhi_ctx_) ||
        (!frame || frame->format != AV_PIX_FMT_D3D11 || !frame->hw_frames_ctx ||
         !frame->hw_frames_ctx->data))
        return 0;

    {
        const auto frames_ctx = reinterpret_cast<AVHWFramesContext *>(frame->hw_frames_ctx->data);
        const auto hwctx      = static_cast<AVD3D11VADeviceContext *>(frames_ctx->device_ctx->hwctx);

        hwctx->lock(hwctx->lock_ctx);
        defer(hwctx->unlock(hwctx->lock_ctx););

        const auto texture = reinterpret_cast<ID3D11Texture2D *>(frame->data[0]);
        const auto index   = static_cast<int>(reinterpret_cast<intptr_t>(frame->data[1]));
        QSize      size{ frame->width, frame->height };

        if (!bridge_.to_shared(hwctx->device, hwctx->device_context, texture, index, size)) return 0;
    }

    const auto output = bridge_.from_shared(rhi_device_, rhi_ctx_);

    return reinterpret_cast<uint64_t>(output.get());
}

#endif