#ifndef CAPTURER_WIN_WGC_H
#define CAPTURER_WIN_WGC_H

#ifdef _WIN32

#include <d3d11.h>
#include <windows.graphics.capture.h>
#include <windows.graphics.capture.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Metadata.h>
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
    inline bool IsSupported()
    {
        return winrt::Windows::Graphics::Capture::GraphicsCaptureSession::IsSupported();
    }

    inline bool IsCursoToggleSupported()
    {
        return winrt::Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent(
            L"Windows.Graphics.Capture.GraphicsCaptureSession", L"IsCursorCaptureEnabled");
    }

    inline bool IsBorderToggleSupported()
    {
        return winrt::Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent(
            L"Windows.Graphics.Capture.GraphicsCaptureSession", L"IsBorderRequired");
    }

    inline auto CreateCaptureItem(HWND hwnd)
    {
        auto factory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem,
                                                     ::IGraphicsCaptureItemInterop>();
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem item = { nullptr };
        winrt::check_hresult(factory->CreateForWindow(
            hwnd, winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(),
            winrt::put_abi(item)));

        return item;
    }

    inline auto CreateCaptureItem(HMONITOR hmon)
    {
        auto factory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem,
                                                     ::IGraphicsCaptureItemInterop>();

        winrt::Windows::Graphics::Capture::GraphicsCaptureItem item = { nullptr };
        winrt::check_hresult(factory->CreateForMonitor(
            hmon, winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(),
            winrt::put_abi(item)));

        return item;
    }

    // exclude a window from capturing
    inline void ExcludeWindow(HWND hwnd)
    {
        DWORD affinity{};
        if (::GetWindowDisplayAffinity(hwnd, &affinity)) {
            if (affinity != WDA_EXCLUDEFROMCAPTURE) {
                // The window is displayed only on a monitor.
                // Everywhere else, the window does not appear at all.
                ::SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
            }
        }
    }

    // create a D3D11 Device
    inline auto CreateD3D11Device()
    {
        winrt::com_ptr<::ID3D11Device> device{};

        for (const auto DRIVER_TYPE : { D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP }) {
            if (SUCCEEDED(::D3D11CreateDevice(nullptr, DRIVER_TYPE, nullptr,
                                              D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
                                              D3D11_SDK_VERSION, device.put(), nullptr, nullptr))) {
                break;
            }
        }

        return device;
    }

    inline auto CreateDirect3DDevice(const winrt::com_ptr<ID3D11Device>& device)
    {
        // If you're using C++/WinRT, then to move back and forth between IDirect3DDevice and IDXGIDevice,
        // use the IDirect3DDxgiInterfaceAccess::GetInterface and CreateDirect3D11DeviceFromDXGIDevice
        // functions.
        //
        // This represents an IDXGIDevice, and can be used to interop between Windows Runtime components
        // that need to exchange IDXGIDevice references.
        winrt::com_ptr<::IInspectable> inspectable{};
        const auto                     dxgi_device = device.as<IDXGIDevice>();
        winrt::check_hresult(::CreateDirect3D11DeviceFromDXGIDevice(dxgi_device.get(), inspectable.put()));
        // winrt d3d11device
        return inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
    }

    // get interface
    template<typename T>
    winrt::com_ptr<T> GetInterfaceFrom(const winrt::Windows::Foundation::IInspectable& object)
    {
        auto              access = object.as<::IDirect3DDxgiInterfaceAccess>();
        winrt::com_ptr<T> result;
        winrt::check_hresult(access->GetInterface(winrt::guid_of<T>(), result.put_void()));

        return result;
    }
} // namespace wgc

#endif

#endif //! CAPTURER_WIN_WGC_H
