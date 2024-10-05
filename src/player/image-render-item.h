#ifndef CAPTURER_IMAGE_RENDER_ITEM_H
#define CAPTURER_IMAGE_RENDER_ITEM_H

#include "libcap/ffmpeg-wrapper.h"
#include "libcap/media.h"
#include "render-item.h"

class ImageRenderItem final : public IRenderItem
{
public:
    bool attach(const std::any&) override;

    QSize size() const override;

    void hdr(bool) override;

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
    std::vector<std::unique_ptr<QRhiTexture>>   planes_{};

    QMatrix4x4 mvp_{};

    av::vformat_t fmt_{ .pix_fmt = AV_PIX_FMT_YUV420P };
    av::frame     frame_{};
    av::frame     frame_slots_[4]{};

    std::atomic<bool> hdr_{};

    std::atomic<bool> uploaded_{};
    std::atomic<bool> created_{};
};

#endif //! CAPTURER_IMAGE_RENDER_ITEM_H