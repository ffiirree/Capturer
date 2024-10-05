#ifndef TEXTURE_CONVERTER_H
#define TEXTURE_CONVERTER_H

#include "libcap/ffmpeg-wrapper.h"

#include <cstdint>

#ifdef _WIN32

#include <d3d11.h>
#include <d3d11_1.h>
#include <QSize>
#include <winrt/base.h>

#endif

class QRhi;

class TextureConverter
{
public:
    explicit TextureConverter(QRhi *rhi)
        : rhi(rhi)
    {}

    virtual ~TextureConverter() = default;

    virtual uint64_t texture(av::frame&) { return 0; }

    QRhi *rhi{};
};

#ifdef _WIN32

class TextureBridge
{
public:
    bool to(ID3D11Device *dev, ID3D11DeviceContext *ctx, ID3D11Texture2D *tex, UINT index,
            const QSize& size);
    winrt::com_ptr<ID3D11Texture2D> from(const winrt::com_ptr<ID3D11Device1>&       dev,
                                         const winrt::com_ptr<ID3D11DeviceContext>& ctx);

private:
    bool ensure_source(ID3D11Device *dev, ID3D11Texture2D *tex, const QSize& size);
    bool is_source_initialized(ID3D11Device *dev, ID3D11Texture2D *tex, const QSize& size);
    bool ensure_sink(const winrt::com_ptr<ID3D11Device1>& dev);

    winrt::handle shared_handle{};

    const UINT                      SRCKEY = 0;
    winrt::com_ptr<ID3D11Texture2D> src{};
    winrt::com_ptr<IDXGIKeyedMutex> src_mtx{};

    const UINT                      DSTKEY = 0;
    winrt::com_ptr<ID3D11Device1>   dst_device{};
    winrt::com_ptr<ID3D11Texture2D> dst{};
    winrt::com_ptr<IDXGIKeyedMutex> dst_mtx{};

    winrt::com_ptr<ID3D11Texture2D> out{};
};

class D3D11TextureConverter : public TextureConverter
{
public:
    explicit D3D11TextureConverter(QRhi *rhi);

    uint64_t texture(av::frame& frame) override;

private:
    winrt::com_ptr<ID3D11Device1>       rhi_device_{};
    winrt::com_ptr<ID3D11DeviceContext> rhi_ctx_{};
    TextureBridge                       bridge_{};
};

#endif

#endif //! TEXTURE_CONVERTER_H