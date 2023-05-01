#ifdef _WIN32

#ifndef WIN_WGC_H
#define WIN_WGC_H

#include <d3d11.h>
#include <windows.graphics.capture.h>
#include <windows.graphics.capture.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>

extern "C" {
HRESULT __stdcall CreateDirect3D11DeviceFromDXGIDevice(::IDXGIDevice *, ::IInspectable **);

HRESULT __stdcall CreateDirect3D11SurfaceFromDXGISurface(::IDXGISurface *, ::IInspectable **);
}

struct __declspec(uuid("A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1")) IDirect3DDxgiInterfaceAccess : ::IUnknown
{
    virtual HRESULT __stdcall GetInterface(GUID const& id, void **object) = 0;
};

namespace wgc
{
    // check whether support Windows Graphics Capture
    inline bool is_supported()
    {
        return winrt::Windows::Graphics::Capture::GraphicsCaptureSession::IsSupported();
    }

    inline winrt::com_ptr<::ID3D11Device> create_d3d11device()
    {
        winrt::com_ptr<::ID3D11Device> device{};

        for (const auto DRIVER_TYPE : { D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP }) {
            if (SUCCEEDED(D3D11CreateDevice(nullptr, DRIVER_TYPE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                            nullptr, 0, D3D11_SDK_VERSION, device.put(), nullptr,
                                            nullptr))) {
                break;
            }
        }

        return device;
    }

    template<typename T>
    winrt::com_ptr<T> get_interface_from(winrt::Windows::Foundation::IInspectable const& object)
    {
        auto access = object.as<::IDirect3DDxgiInterfaceAccess>();
        winrt::com_ptr<T> result;
        winrt::check_hresult(access->GetInterface(winrt::guid_of<T>(), result.put_void()));
        return result;
    }

    inline winrt::Windows::Graphics::Capture::GraphicsCaptureItem create_capture_item_for_window(HWND hwnd)
    {
        if (!hwnd) return nullptr;

        auto factory =
            winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>()
                .as<IGraphicsCaptureItemInterop>();
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem item = nullptr;

        try {
            if (FAILED(factory->CreateForWindow(
                    hwnd, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
                    reinterpret_cast<void **>(winrt::put_abi(item)))))
                return nullptr;
        }
        catch (...) {
            return nullptr;
        }
        return item;
    }

    inline winrt::Windows::Graphics::Capture::GraphicsCaptureItem
    create_capture_item_for_monitor(HMONITOR monitor)
    {
        if (!monitor) return nullptr;

        auto factory =
            winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>()
                .as<IGraphicsCaptureItemInterop>();
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem item = nullptr;

        try {
            if (FAILED(factory->CreateForMonitor(
                    monitor, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
                    reinterpret_cast<void **>(winrt::put_abi(item)))))
                return nullptr;
        }
        catch (...) {
            return nullptr;
        }
        return item;
    }
} // namespace wgc

#endif //! WIN_WGC_H

#endif