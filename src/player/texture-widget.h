#ifndef CAPTURER_TEXTURE_WIDGET_H
#define CAPTURER_TEXTURE_WIDGET_H

#include "libcap/ffmpeg-wrapper.h"
#include "libcap/media.h"

#include <array>
#include <memory>
#include <QRhiWidget>
#include <rhi/qrhi.h>

struct TextureDescription
{
    int32_t             xfactor{};
    int32_t             yfactor{};
    QRhiTexture::Format format{};
    uint32_t            bytes{};
};

constexpr int MAX_PLANES = 4;

class TextureWidget : public QRhiWidget
{
    Q_OBJECT
public:
    explicit TextureWidget(QWidget *parent = nullptr);

    void initialize(QRhiCommandBuffer *cb) override;
    void render(QRhiCommandBuffer *cb) override;

public:
    void present(const av::frame& frame);

    static std::vector<AVPixelFormat> PixelFormats();

    static bool IsSupported(AVPixelFormat fmt);

signals:
    void arrived();

private:
    void UpdateTextures(QRhiResourceUpdateBatch *rub);
    void SetupPipeline(QRhiGraphicsPipeline *pipeline, QRhiShaderResourceBindings *bindings,
                       AVPixelFormat pix_fmt);

private:
    // QRhi @{
    QRhi *rhi_{};

    std::unique_ptr<QRhiBuffer>                          vbuf_{};
    std::unique_ptr<QRhiBuffer>                          ubuf_{};
    std::unique_ptr<QRhiShaderResourceBindings>          srb_{};
    std::unique_ptr<QRhiGraphicsPipeline>                pipeline_{};
    QMatrix4x4                                           mvp_{};
    std::unique_ptr<QRhiSampler>                         sampler_{};
    std::array<std::unique_ptr<QRhiTexture>, MAX_PLANES> planes_{};

    std::unique_ptr<QRhiGraphicsPipeline> sub_pipeline_{};
    std::unique_ptr<QRhiTexture>          subtitle_{};
    // @}

    av::vformat_t fmt_{ .width = 1920, .height = 1080, .pix_fmt = AV_PIX_FMT_YUV420P };

    // frame
    av::frame frame_{};

    std::mutex mtx_;

    std::atomic<bool> config_dirty_{ true };
};

#endif //! CAPTURER_TEXTURE_WIDGET_H