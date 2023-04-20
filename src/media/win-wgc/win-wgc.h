#ifndef WIN_WGC_H
#define WIN_WGC_H

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Composition.Desktop.h>
#include <winrt/Windows.UI.Popups.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3d11.h>
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <d2d1_3.h>
#include <wincodec.h>

#include <windows.graphics.capture.interop.h>
#include <windows.graphics.capture.h>

#include "composition.interop.h"
#include "direct3d11.interop.h"
#include "d3dHelpers.h"

namespace wgc 
{
    inline bool is_supported()
    {
        return winrt::Windows::Graphics::Capture::GraphicsCaptureSession::IsSupported();
    }

    inline auto create_d3d11_device()
    {
        winrt::com_ptr<ID3D11Device> device;

        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#ifdef _DEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        winrt::check_hresult(D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            flags,
            nullptr, 0,
            D3D11_SDK_VERSION,
            device.put(),
            nullptr,
            nullptr
        ));

        return device;
    }
}

#endif //!WIN_WGC_H