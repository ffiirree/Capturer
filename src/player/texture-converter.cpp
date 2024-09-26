#include "texture-converter.h"

#include <rhi/qrhi.h>

#ifdef _WIN32

#include "probe/defer.h"

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
}

winrt::com_ptr<ID3D11Device1> GetD3DDeviceFromQRhi(QRhi *rhi)
{
    const auto native = static_cast<const QRhiD3D11NativeHandles *>(rhi->nativeHandles());
    if (!native) return {};

    winrt::com_ptr<ID3D11Device> rhi_device{};
    rhi_device.copy_from(static_cast<ID3D11Device *>(native->dev));

    //
    winrt::com_ptr<ID3D11Device1> dev1;
    if (rhi_device.try_as(dev1) != S_OK) return nullptr;

    return dev1;
}

///

bool TextureBridge::ensure_source(ID3D11Device *dev, ID3D11Texture2D *tex, const QSize& size)
{
    if (!is_source_initialized(dev, tex, size)) {
        shared_handle.close();

        CD3D11_TEXTURE2D_DESC desc{};
        tex->GetDesc(&desc);

        const UINT w = static_cast<UINT>(size.width());
        const UINT h = static_cast<UINT>(size.height());

        CD3D11_TEXTURE2D_DESC texDesc{ desc.Format, w, h };
        texDesc.MipLevels = 1;
        texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;

        if (dev->CreateTexture2D(&texDesc, nullptr, src.put()) != S_OK) return false;

        winrt::com_ptr<IDXGIResource1> res;
        if (src.try_as(res) != S_OK) return false;

        const HRESULT hr =
            res->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ, nullptr, shared_handle.put());

        if (hr != S_OK || !shared_handle) return false;

        if (src.try_as(src_mtx) != S_OK || !src_mtx) return false;

        dst     = nullptr;
        dst_mtx = nullptr;

        return true;
    }

    return true;
}

bool TextureBridge::is_source_initialized(ID3D11Device *dev, ID3D11Texture2D *tex, const QSize& size)
{
    if (!src) return false;

    winrt::com_ptr<ID3D11Device> tex_device{};
    src->GetDevice(tex_device.put());

    if (dev != tex_device.get()) return false;

    CD3D11_TEXTURE2D_DESC source_desc{};
    tex->GetDesc(&source_desc);

    CD3D11_TEXTURE2D_DESC current_desc{};
    src->GetDesc(&current_desc);

    if (source_desc.Format != current_desc.Format) return false;

    const UINT width  = static_cast<UINT>(size.width());
    const UINT height = static_cast<UINT>(size.height());

    if (current_desc.Width != width || current_desc.Height != height) return false;

    return true;
}

bool TextureBridge::ensure_sink(const winrt::com_ptr<ID3D11Device1>& dev)
{
    if (dst_device != dev) {
        dst        = nullptr;
        dst_device = dev;
    }

    if (dst) return true;

    if (dst_device->OpenSharedResource1(shared_handle.get(), IID_PPV_ARGS(&dst)) != S_OK) {
        return false;
    }

    CD3D11_TEXTURE2D_DESC desc{};
    dst->GetDesc(&desc);

    desc.MiscFlags = 0;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    if (dst_device->CreateTexture2D(&desc, nullptr, out.put()) != S_OK) return false;

    if (dst.try_as(dst_mtx) != S_OK) return false;

    return true;
}

bool TextureBridge::to(ID3D11Device *dev, ID3D11DeviceContext *ctx, ID3D11Texture2D *tex, UINT index,
                       const QSize& size)
{

    if (!ensure_source(dev, tex, size)) return false;

    // flush to ensure that texture is fully updated before we share it.
    ctx->Flush();

    if (src_mtx->AcquireSync(SRCKEY, INFINITE) != S_OK) return false;

    const UINT w = static_cast<UINT>(size.width());
    const UINT h = static_cast<UINT>(size.height());

    const D3D11_BOX crop{ 0, 0, 0, w, h, 1 };
    ctx->CopySubresourceRegion(src.get(), 0, 0, 0, 0, tex, index, &crop);

    src_mtx->ReleaseSync(DSTKEY);

    return true;
}

winrt::com_ptr<ID3D11Texture2D> TextureBridge::from(const winrt::com_ptr<ID3D11Device1>&       dev,
                                                    const winrt::com_ptr<ID3D11DeviceContext>& ctx)
{
    if (!ensure_sink(dev)) return {};

    if (dst_mtx->AcquireSync(DSTKEY, INFINITE) != S_OK) return {};

    ctx->CopySubresourceRegion(out.get(), 0, 0, 0, 0, dst.get(), 0, nullptr);

    dst_mtx->ReleaseSync(SRCKEY);

    return out;
}

///

D3D11TextureConverter::D3D11TextureConverter(QRhi *rhi)
    : TextureConverter(rhi), rhi_device_(GetD3DDeviceFromQRhi(rhi))
{
    if (rhi_device_) rhi_device_->GetImmediateContext(rhi_ctx_.put());
}

int64_t D3D11TextureConverter::texture(av::frame& frame)
{
    if (!rhi_device_) return 0;
    if (!frame || !frame->hw_frames_ctx || frame->format != AV_PIX_FMT_D3D11) return 0;

    const auto frames_ctx = reinterpret_cast<AVHWFramesContext *>(frame->hw_frames_ctx->data);
    if (!frames_ctx) return 0;

    const auto device_ctx = frames_ctx->device_ctx;

    if (!device_ctx || device_ctx->type != AV_HWDEVICE_TYPE_D3D11VA) return 0;

    const auto texture = reinterpret_cast<ID3D11Texture2D *>(frame->data[0]);
    const auto index   = static_cast<int>(reinterpret_cast<intptr_t>(frame->data[1]));

    if (rhi->backend() == QRhi::D3D11) {
        {
            const auto *avd3d11_ctx = static_cast<AVD3D11VADeviceContext *>(device_ctx->hwctx);

            if (!avd3d11_ctx) return 0;

            avd3d11_ctx->lock(avd3d11_ctx->lock_ctx);
            defer([&] { avd3d11_ctx->unlock(avd3d11_ctx->lock_ctx); });

            QSize size{ frame->width, frame->height };

            if (!bridge_.to(avd3d11_ctx->device, avd3d11_ctx->device_context, texture, index, size))
                return 0;
        }
    }

    const auto output = bridge_.from(rhi_device_, rhi_ctx_);

    return reinterpret_cast<int64_t>(output.get());
}

#endif