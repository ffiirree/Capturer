#pragma once
#include "composition.interop.h"

inline auto CreateD3DDevice()
{
    winrt::com_ptr<ID3D11Device> device;

    auto hr = D3D11CreateDevice(
        nullptr, 
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr, 
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr, 0,
        D3D11_SDK_VERSION,
        device.put(),
        nullptr,
        nullptr
    );

    if (DXGI_ERROR_UNSUPPORTED == hr)
    {
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr, 0,
            D3D11_SDK_VERSION,
            device.put(),
            nullptr,
            nullptr
        );
    }

    winrt::check_hresult(hr);
    return device;
}
