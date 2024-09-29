#ifndef CAPTURER_TEXTURE_WIDGET_RHI_H
#define CAPTURER_TEXTURE_WIDGET_RHI_H

#include "libcap/ffmpeg-wrapper.h"
#include "libcap/media.h"
#include "subtitle.h"

#include <array>
#include <ass/ass.h>
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

class TextureRhiWidget : public QRhiWidget
{
    Q_OBJECT
public:
    explicit TextureRhiWidget(QWidget *parent = nullptr);

    void initialize(QRhiCommandBuffer *cb) override;
    void render(QRhiCommandBuffer *cb) override;

    QSize pixelSize() const { return (rhi_ && renderTarget()) ? renderTarget()->pixelSize() : QSize{}; }

    QSize framePixelSize() const
    {
        const auto sz = QSize{ fmt_.width, fmt_.height };
        return (rhi_ && renderTarget()) ? sz.scaled(renderTarget()->pixelSize(), Qt::KeepAspectRatio) : sz;
    }

public:
    void present(const av::frame& frame);

    void present(const std::list<Subtitle>& subtitles, int changed);

    static std::vector<AVPixelFormat> PixelFormats();

    static bool IsSupported(AVPixelFormat fmt);

signals:
    void updateRequest();

private:
    void UpdateTextures(QRhiResourceUpdateBatch *rub);
    void SetupPipeline(QRhiGraphicsPipeline *pipeline, QRhiShaderResourceBindings *bindings,
                       const QString& frag);

private:
    // QRhi @{
    QRhi *rhi_{};

    QMatrix4x4                   mvp_{};
    std::unique_ptr<QRhiSampler> sampler_{};

    // video frame
    std::unique_ptr<QRhiGraphicsPipeline>                pipeline_{};
    std::unique_ptr<QRhiBuffer>                          vbuf_{};
    std::unique_ptr<QRhiBuffer>                          ubuf_{};
    std::unique_ptr<QRhiShaderResourceBindings>          srb_{};
    std::array<std::unique_ptr<QRhiTexture>, MAX_PLANES> planes_{};

    // subtitles
    struct RenderItem
    {
        std::unique_ptr<QRhiBuffer>                 vbuf{};
        std::unique_ptr<QRhiBuffer>                 ubuf{};
        std::unique_ptr<QRhiTexture>                tex{};
        std::unique_ptr<QRhiShaderResourceBindings> srb{};
        std::unique_ptr<QRhiGraphicsPipeline>       pipeline{};
    };
    std::vector<RenderItem>  items_{};
    QRhiResourceUpdateBatch *sub_rub_{};

    std::list<Subtitle> subtitles_{};
    // @}

    av::vformat_t fmt_{ .pix_fmt = AV_PIX_FMT_YUV420P };

    // frame
    av::frame frame_{};

    av::frame frame_slots_[4]{};

    std::mutex mtx_;
    std::mutex sub_mtx_{};

    std::atomic<bool> format_changed_{ true };
    std::atomic<bool> subtitles_changed_{ true };
};

#endif //! CAPTURER_TEXTURE_WIDGET_RHI_H