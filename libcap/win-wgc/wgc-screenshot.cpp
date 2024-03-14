#include "libcap/win-wgc/wgc-screenshot.h"

#ifdef _WIN32

#include "libcap/hwaccel.h"
#include "libcap/win-wgc/win-wgc.h"
#include "logging.h"

#include <condition_variable>
#include <mutex>
#include <winrt/base.h>

extern "C" {
#include <libavutil/hwcontext_d3d11va.h>
}

namespace winrt
{
    using namespace winrt::Windows::Graphics::Capture;
    using namespace winrt::Windows::Graphics::DirectX;
    using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
} // namespace winrt

namespace wgc
{
    static winrt::com_ptr<ID3D11Texture2D>
    screenshot(const winrt::Windows::Graphics::Capture::GraphicsCaptureItem& item)
    {
        auto hwctx = av::hwaccel::get_context(AV_HWDEVICE_TYPE_D3D11VA);
        winrt::check_pointer(hwctx.get());

        winrt::com_ptr<ID3D11Device>        device{};
        winrt::com_ptr<ID3D11DeviceContext> context{};
        context.copy_from(hwctx->native_context<AVD3D11VADeviceContext>());
        device.copy_from(hwctx->native_device<AVD3D11VADeviceContext>());

        auto winrt_device = wgc::CreateDirect3DDevice(device);

        auto pool = winrt::Direct3D11CaptureFramePool::CreateFreeThreaded(
            winrt_device, winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized, 1, item.Size());

        auto session = pool.CreateCaptureSession(item);

        std::mutex              mtx{};
        std::condition_variable cv{};
        std::atomic<bool>       arrived{};

        winrt::com_ptr<ID3D11Texture2D> frame{};
        pool.FrameArrived([&](auto& sender, auto&) {
            frame = wgc::GetInterfaceFrom<::ID3D11Texture2D>(sender.TryGetNextFrame().Surface());

            arrived = true;
            cv.notify_one();
        });

        session.StartCapture();

        std::unique_lock lock(mtx);
        cv.wait(lock, [&]() -> bool { return arrived; });

        session.Close();
        pool.Close();

        return frame;
    }

    winrt::com_ptr<ID3D11Texture2D> screenshot(HWND hwnd)
    {
        return screenshot(wgc::CreateCaptureItem(hwnd));
    }

    winrt::com_ptr<ID3D11Texture2D> screenshot(HMONITOR hmon)
    {
        return screenshot(wgc::CreateCaptureItem(hmon));
    }

} // namespace wgc

#endif