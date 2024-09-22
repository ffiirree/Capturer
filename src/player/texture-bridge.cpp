#include "texture-bridge.h"

#include <d3d11.h>
#include <winrt/base.h>

extern "C" {
#include <libavutil/hwcontext_d3d11va.h>
}

class D3D11TextureBridge
{
    const UINT                      SRCKEY = 0;
    winrt::com_ptr<ID3D11Texture2D> src{};
    winrt::com_ptr<IDXGIKeyedMutex> src_mtx{};

    const UINT                      DSTKEY = 0;
    winrt::com_ptr<ID3D11Texture2D> dst{};
    winrt::com_ptr<IDXGIKeyedMutex> dst_mtx{};

    winrt::com_ptr<ID3D11Texture2D> out{};
};

bool TextureBridge::from(const av::frame& frame) { return true; }

int64_t TextureBridge::handle() { return 0; }