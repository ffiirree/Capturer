#ifdef _WIN32

#include "libcap/win-wgc/win-wgc.h"

#include <winrt/Windows.Foundation.Metadata.h>

using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Foundation::Metadata;

namespace wgc
{
    // check whether support Windows Graphics Capture
    bool is_supported() { return GraphicsCaptureSession::IsSupported(); }

    void exclude(HWND winid)
    {
        DWORD affinity{};
        if (::GetWindowDisplayAffinity(winid, &affinity)) {
            if (affinity != WDA_EXCLUDEFROMCAPTURE) {
                // The window is displayed only on a monitor. Everywhere else, the window does not appear at all.
                ::SetWindowDisplayAffinity(winid, WDA_EXCLUDEFROMCAPTURE);
            }
        }
    }

    winrt::com_ptr<::ID3D11Device> create_d3d11device()
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

    GraphicsCaptureItem create_capture_item_for_window(HWND hwnd)
    {
        if (!hwnd) return nullptr;

        auto factory = winrt::get_activation_factory<GraphicsCaptureItem, ::IGraphicsCaptureItemInterop>();
        GraphicsCaptureItem item = nullptr;

        try {
            winrt::check_hresult(factory->CreateForWindow(hwnd, winrt::guid_of<GraphicsCaptureItem>(),
                                                          winrt::put_abi(item)));
        }
        catch (...) {
            return nullptr;
        }

        return item;
    }

    GraphicsCaptureItem create_capture_item_for_monitor(HMONITOR monitor)
    {
        if (!monitor) return nullptr;

        auto factory = winrt::get_activation_factory<GraphicsCaptureItem, ::IGraphicsCaptureItemInterop>();
        GraphicsCaptureItem item = nullptr;

        try {
            winrt::check_hresult(factory->CreateForMonitor(monitor, winrt::guid_of<GraphicsCaptureItem>(),
                                                           winrt::put_abi(item)));
        }
        catch (...) {
            return nullptr;
        }

        return item;
    }

    bool is_cursor_toggle_supported()
    {
        try {
            return ApiInformation::IsPropertyPresent(L"Windows.Graphics.Capture.GraphicsCaptureSession",
                                                     L"IsCursorCaptureEnabled");
        }
        catch (...) {
            return false;
        }
    }

    bool is_border_toggle_supported()
    {
        try {
            return ApiInformation::IsPropertyPresent(L"Windows.Graphics.Capture.GraphicsCaptureSession",
                                                     L"IsBorderRequired");
        }
        catch (...) {
            return false;
        }
    }
} // namespace wgc

#endif