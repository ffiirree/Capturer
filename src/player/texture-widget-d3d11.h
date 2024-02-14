#ifndef CAPTURER_TEXTURE_WIDGET_D3D11_H
#define CAPTURER_TEXTURE_WIDGET_D3D11_H

#include <mutex>
#include <QWidget>

#ifdef Q_OS_WIN

#include <d3d11.h>
#include <DirectXMath.h>
#include <libcap/ffmpeg-wrapper.h>
#include <libcap/media.h>
#include <memory>
#include <winrt/base.h>

class TextureD3D11Widget final : public QWidget
{
    Q_OBJECT
public:
    explicit TextureD3D11Widget(QWidget *parent = nullptr);

    ~TextureD3D11Widget() override;

    void present(const av::frame& frame);

    std::vector<AVPixelFormat> pix_fmts() const;

    bool isSupported(AVPixelFormat) const;

    int setFormat(const av::vformat_t& vfmt);

    av::vformat_t format() const { return format_; }

    AVPixelFormat pix_fmt() const { return format_.pix_fmt; }

    AVRational SAR() const;
    AVRational DAR() const;

    QPaintEngine *paintEngine() const override { return nullptr; }

protected:
    void showEvent(QShowEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

    void initializeD3D11();
    void resizeD3D11(int w, int h);
    void paintD3D11();

private:
    void UpdateViewport(int w, int h);
    void CreateTextureAndShaderResourceViews();
    void SetupRenderTargets();

    av::vformat_t format_{ .pix_fmt = AV_PIX_FMT_YUV420P };

    // frame
    av::frame frame_{};

    std::mutex mtx_;

    std::atomic<bool> config_dirty_{ true };

    // D3D11 @{
    winrt::com_ptr<ID3D11Device>        device_{};
    winrt::com_ptr<ID3D11DeviceContext> context_{ nullptr };
    winrt::com_ptr<IDXGISwapChain>      swap_chain_{ nullptr };

    winrt::com_ptr<ID3D11Texture2D>        texture_{};
    winrt::com_ptr<ID3D11RenderTargetView> rtv_{};

    winrt::com_ptr<ID3D11Buffer>       vertex_buffer_{};
    winrt::com_ptr<ID3D11InputLayout>  vertex_layout_{};
    winrt::com_ptr<ID3D11VertexShader> vertex_shader_{};

    winrt::com_ptr<ID3D11SamplerState> sampler_{};
    winrt::com_ptr<ID3D11PixelShader>  pixel_shader_{};

    winrt::com_ptr<ID3D11ShaderResourceView> srv0_{};
    winrt::com_ptr<ID3D11ShaderResourceView> srv1_{};
    winrt::com_ptr<ID3D11ShaderResourceView> srv2_{};

    DirectX::XMMATRIX            proj_{};
    winrt::com_ptr<ID3D11Buffer> proj_buffer_{};
    D3D11_VIEWPORT               viewport_{};
    // @}
};

#endif // Q_OS_WIN
#endif // !CAPTURER_TEXTURE_WIDGET_D3D11_H
