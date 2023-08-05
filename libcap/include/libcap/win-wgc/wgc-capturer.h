#ifdef _WIN32

#ifndef CAPTURER_WGC_CAPTURER_H
#define CAPTURER_WGC_CAPTURER_H

#include "libcap/hwaccel.h"
#include "libcap/producer.h"
#include "libcap/ringvector.h"
#include "win-wgc.h"

class WindowsGraphicsCapturer : public Producer<AVFrame>
{
public:
    ~WindowsGraphicsCapturer() { reset(); }

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

    int produce(AVFrame *, AVMediaType) override;
    bool empty(AVMediaType) override;

    bool has(AVMediaType mt) const override;
    std::string format_str(AVMediaType) const override;
    AVRational time_base(AVMediaType) const override;

    std::vector<av::vformat_t> vformats() const override;

private:
    void OnFrameArrived(const winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool& sender,
                        const winrt::Windows::Foundation::IInspectable& args);

    void OnClosed(const winrt::Windows::Graphics::Capture::GraphicsCaptureItem&,
                  const winrt::Windows::Foundation::IInspectable&);

    int InitializeHWFramesContext();

    void parse_options(std::map<std::string, std::string>&);

private:
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item_{ nullptr };
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice device_{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession session_{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool frame_pool_{ nullptr };

    winrt::com_ptr<ID3D11DeviceContext> d3d11_ctx_{ nullptr };

    winrt::Windows::Graphics::Capture::GraphicsCaptureItem::Closed_revoker closed_;
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::FrameArrived_revoker frame_arrived_;

    AVBufferRef *device_ref_{ nullptr };

    AVBufferRef *frames_ref_{ nullptr };
    AVHWFramesContext *frames_ctx_{ nullptr };

    winrt::com_ptr<ID3D11Texture2D> resize_texture_{};
    winrt::com_ptr<ID3D11RenderTargetView> resize_render_target_{};
    winrt::com_ptr<ID3D11ShaderResourceView> resize_shader_view_{};

    av::frame frame_{};
    uint32_t frame_number_{};

    // options @{
    bool draw_mouse_{ true };
    bool show_region_{ true };
    D3D11_BOX box_{ .front = 0, .back = 1 };
    // @}

    RingVector<AVFrame *, 4> buffer_{
        []() { return av_frame_alloc(); },
        [](AVFrame **frame) { av_frame_free(frame); },
    };
};

#endif //! CAPTURER_WGC_CAPTURER_H

#endif