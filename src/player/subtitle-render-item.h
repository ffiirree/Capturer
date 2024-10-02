#ifndef CAPTURER_SUBTITLE_RENDER_ITEM_H
#define CAPTURER_SUBTITLE_RENDER_ITEM_H

#include "render-item.h"
#include "subtitle.h"

class SubtitleRenderItem final : public IRenderItem
{
public:
    bool attach(const std::any&) override;

    void create(QRhi *rhi, QRhiRenderTarget *rt) override;
    void upload(QRhiResourceUpdateBatch *rub, float scale_x, float scale_y) override;
    void draw(QRhiCommandBuffer *cb, const QRhiViewport& viewport) override;

private:
    mutable std::mutex mtx_{};

    QRhi *rhi_{};

    std::unique_ptr<QRhiGraphicsPipeline>       pipeline_{};
    std::unique_ptr<QRhiBuffer>                 vbuf_{};
    std::unique_ptr<QRhiBuffer>                 ubuf_{};
    std::unique_ptr<QRhiSampler>                sampler_{};
    std::unique_ptr<QRhiShaderResourceBindings> srb_{};
    std::unique_ptr<QRhiTexture>                texture_{};

    QMatrix4x4 mvp_{};

    Subtitle subtitle_{};
    Subtitle subtitle_slots_[4]{};

    std::atomic<bool> uploaded_{};
    std::atomic<bool> created_{};
};

#endif //! CAPTURER_SUBTITLE_RENDER_ITEM_H