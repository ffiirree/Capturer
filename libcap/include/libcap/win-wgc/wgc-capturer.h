#ifndef CAPTURER_WGC_CAPTURER_H
#define CAPTURER_WGC_CAPTURER_H

#ifdef _WIN32

#include "libcap/hwaccel.h"
#include "libcap/screen-capturer.h"
#include "win-wgc.h"

#include <mutex>

class WindowsGraphicsCapturer final : public ScreenCapturer
{
public:
    ~WindowsGraphicsCapturer() override;

    int open(const std::string& name, std::map<std::string, std::string> options) override;

    int start() override;

    void stop() override;

    std::vector<av::vformat_t> video_formats() const override;

private:
    void OnFrameArrived(const winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool& sender,
                        const winrt::Windows::Foundation::IInspectable&                      args);

    void OnClosed(const winrt::Windows::Graphics::Capture::GraphicsCaptureItem&,
                  const winrt::Windows::Foundation::IInspectable&);

    // for resizing texture
    int InitializeResizingResources();

    int InitializeHWFramesContext();

private:
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item_{ nullptr };

    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice winrt_device_{ nullptr };

    winrt::Windows::Graphics::Capture::GraphicsCaptureSession     session_{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool frame_pool_{ nullptr };
    winrt::Windows::Graphics::SizeInt32                           size_{}; // frame pool size

    winrt::Windows::Graphics::Capture::GraphicsCaptureItem::Closed_revoker              onclosed_;
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::FrameArrived_revoker onarrived_;

    // ffmpeg hardware device context
    std::shared_ptr<av::hwaccel::context_t> hwctx_{};

    // D3D11
    winrt::com_ptr<ID3D11Device>        device_{}; // same as hwctx_->native_device()
    winrt::com_ptr<ID3D11DeviceContext> context_{};

    // @{ Resize
    winrt::com_ptr<ID3D11Buffer>       vertex_buffer_{};
    winrt::com_ptr<ID3D11InputLayout>  vertex_layout_{};
    winrt::com_ptr<ID3D11VertexShader> vertex_shader_{};

    winrt::com_ptr<ID3D11SamplerState> sampler_{};
    winrt::com_ptr<ID3D11PixelShader>  pixel_shader_{};

    winrt::com_ptr<ID3D11Texture2D>          source_{};
    winrt::com_ptr<ID3D11ShaderResourceView> srv_{};

    winrt::com_ptr<ID3D11Texture2D>        target_{};
    winrt::com_ptr<ID3D11RenderTargetView> rtv_{};

    DirectX::XMMATRIX            proj_{};
    winrt::com_ptr<ID3D11Buffer> proj_buffer_{};
    // @}

    std::mutex mtx_{};

    uint32_t frame_number_{};

    // options @{
    D3D11_BOX box_{ .front = 0, .back = 1 };
    // @}
};

#endif

#endif //! CAPTURER_WGC_CAPTURER_H
