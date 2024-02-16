#ifndef CAPTURER_WGC_CAPTURER_H
#define CAPTURER_WGC_CAPTURER_H

#ifdef _WIN32

#include "libcap/ffmpeg-wrapper.h"
#include "libcap/hwaccel.h"
#include "libcap/producer.h"
#include "libcap/queue.h"
#include "win-wgc.h"

class WindowsGraphicsCapturer final : public Producer<av::frame>
{
    enum mode_t
    {
        monitor,
        window,
    };

public:
    ~WindowsGraphicsCapturer() override { reset(); }

    /**
     * @param name      window  capture: "window=<HWND>"
     *                  display capture: "display=<HMONITOR>"
     *                  desktop capture: "desktop"
     * @param options   framerate      : set the grabbing frame rate.
     *                  offset_x       : set the distance from the left edge of the screen or desktop.
     *                  offset_y       : set the distance from the top edge of the screen or desktop.
     *                  video_size     : set the video frame size. The default is to capture the full screen
     *                                   if "desktop" is selected.
     *                  draw_mouse     : specify whether to draw the mouse pointer, default value is 1.
     *                  show_region    : show grabbed region on screen, default value is 1.
     *
     * @return int      zero on success, a negative code on failure.
     */
    int open(const std::string& name, std::map<std::string, std::string> options) override;

    void reset() override;

    int run() override;

    int  produce(av::frame&, AVMediaType) override;
    bool empty(AVMediaType) override;

    bool        has(AVMediaType mt) const override;
    std::string format_str(AVMediaType) const override;
    AVRational  time_base(AVMediaType) const override;

    std::vector<av::vformat_t> vformats() const override;

private:
    void OnFrameArrived(const winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool& sender,
                        const winrt::Windows::Foundation::IInspectable&                      args);

    void OnClosed(const winrt::Windows::Graphics::Capture::GraphicsCaptureItem&,
                  const winrt::Windows::Foundation::IInspectable&);

    // for resizing texture
    int InitalizeResizingResources();

    int InitializeHWFramesContext();

    void parse_options(std::map<std::string, std::string>&);

private:
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item_{ nullptr };

    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice winrt_device_{ nullptr };

    winrt::Windows::Graphics::Capture::GraphicsCaptureSession     session_{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool frame_pool_{ nullptr };

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

    mode_t mode_{};

    std::mutex mtx_{};

    uint32_t frame_number_{};

    // options @{
    bool      draw_mouse_{ true };
    bool      show_region_{ true };
    D3D11_BOX box_{ .front = 0, .back = 1 };
    // @}

    safe_queue<av::frame> buffer_{ 4 };
};

#endif //! CAPTURER_WGC_CAPTURER_H

#endif