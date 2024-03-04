#ifndef CAPTURER_WIN_WGC_H
#define CAPTURER_WIN_WGC_H

#ifdef _WIN32

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
    // check whether support 'Windows Graphics Capture'
    bool is_supported();

    bool is_cursor_toggle_supported();

    bool is_border_toggle_supported();

    // for HWND Window
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem create_capture_item_for_window(HWND);

    // for monitir
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem create_capture_item_for_monitor(HMONITOR);

    // exclude a window from capturing
    void exclude(HWND winid);

    // create a D3D11 Device
    winrt::com_ptr<::ID3D11Device> create_d3d11device();

    // get interface
    template<typename T>
    winrt::com_ptr<T> get_interface_from(const winrt::Windows::Foundation::IInspectable& object)
    {
        auto              access = object.as<::IDirect3DDxgiInterfaceAccess>();
        winrt::com_ptr<T> result;
        winrt::check_hresult(access->GetInterface(winrt::guid_of<T>(), result.put_void()));
        return result;
    }
} // namespace wgc

#endif

#endif //! CAPTURER_WIN_WGC_H
