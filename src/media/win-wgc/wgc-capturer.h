#ifdef _WIN32

#ifndef WGC_CAPTURER_H
#define WGC_CAPTURER_H

#include "win-wgc.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/hwcontext_d3d11va.h>
#include <libavutil/time.h>
}
#include "hwaccel.h"
#include "producer.h"
#include "ringvector.h"

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

    int produce(AVFrame *, int) override;
    bool empty(int) override;

    bool has(int mt) const override;
    std::string format_str(int) const override;
    AVRational time_base(int) const override;

    std::vector<av::vformat_t> vformats() const override;

    std::vector<av::aformat_t> aformats() const override { return {}; }

private:
    void on_frame_arrived(const winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool& sender,
                          const winrt::Windows::Foundation::IInspectable& args);

    void on_closed(const winrt::Windows::Graphics::Capture::GraphicsCaptureItem&,
                   const winrt::Windows::Foundation::IInspectable&);

    int init_hwframes_ctx();

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

#endif // !WGC_CAPTURER_H

#endif