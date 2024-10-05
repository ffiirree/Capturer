#include "subtitle-render-item.h"

#include "logging.h"
#include "texture-helper.h"

bool SubtitleRenderItem::attach(const std::any& attachment)
{
    try {
        const auto sub = std::any_cast<Subtitle>(attachment);

        std::scoped_lock lock(mtx_);

        subtitle_ = sub;
        uploaded_ = false;
    }
    catch (const std::bad_any_cast& e) {
        loge("failed to cast subtitle: {}", e.what());
        return false;
    }

    return true;
}

QSize SubtitleRenderItem::size() const { return { subtitle_.w.den, subtitle_.h.den }; }

void SubtitleRenderItem::create(QRhi *rhi, QRhiRenderTarget *rt)
{
    if (created_.exchange(true)) return;
    std::scoped_lock lock(mtx_);

    rhi_ = rhi;

    vbuf_.reset(rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, 16 * sizeof(float)));
    vbuf_->create();

    ubuf_.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64 + 64 + 16));
    ubuf_->create();

    sampler_.reset(rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                   QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
    sampler_->create();

    texture_.reset(
        rhi->newTexture((subtitle_.format == AV_PIX_FMT_PAL8) ? QRhiTexture::R8 : QRhiTexture::RGBA8,
                        { subtitle_.w.num, subtitle_.h.num }));
    texture_->create();

    srb_.reset(rhi->newShaderResourceBindings());
    srb_->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            ubuf_.get()),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage,
                                                  texture_.get(), sampler_.get()),

    });
    srb_->create();

    pipeline_.reset(rhi->newGraphicsPipeline());
    pipeline_->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    pipeline_->setShaderStages({
        { QRhiShaderStage::Vertex, av::get_shader(":/src/resources/shaders/vertex.vert.qsb") },
        { QRhiShaderStage::Fragment, av::get_frag_shader({ .pix_fmt = subtitle_.format }, false) },
    });

    QRhiVertexInputLayout layout{};
    layout.setBindings({ 4 * sizeof(float) });
    layout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float) },
    });

    QRhiGraphicsPipeline::TargetBlend blend{
        .enable   = true,
        .srcColor = QRhiGraphicsPipeline::SrcAlpha,
        .dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha,
    };
    pipeline_->setTargetBlends({ blend });

    pipeline_->setVertexInputLayout(layout);
    pipeline_->setShaderResourceBindings(srb_.get());
    pipeline_->setRenderPassDescriptor(rt->renderPassDescriptor());
    pipeline_->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    pipeline_->create();
}

void SubtitleRenderItem::upload(QRhiResourceUpdateBatch *rub, float scale_x, float scale_y)
{
    std::scoped_lock lock(mtx_);

    mvp_.setToIdentity();
    mvp_.scale(scale_x, scale_y);
    rub->updateDynamicBuffer(ubuf_.get(), 0, 64, mvp_.constData());

    if (uploaded_.exchange(true)) return;

    const auto x = subtitle_.x.get<float>();
    const auto y = subtitle_.y.get<float>();
    const auto w = subtitle_.w.get<float>();
    const auto h = subtitle_.h.get<float>();

    const float x1 = std::clamp(2 * x - 1.0f, -1.0f, 1.0f);
    const float y1 = std::clamp(1.0f - 2 * y, -1.0f, 1.0f);
    const float x2 = std::clamp(2 * (x + w) - 1.0f, -1.0f, 1.0f);
    const float y2 = std::clamp(1.0f - 2 * (y + h), -1.0f, 1.0f);

    // clang-format off
    float vs[] = {
        x1, y2,  /* bottom left  */  0.0f, 1.0f, /* top    left  */
        x2, y2,  /* bottom right */  1.0f, 1.0f, /* top    right */
        x1, y1,  /* top    left  */  0.0f, 0.0f, /* bottom left  */
        x2, y1,  /* top    right */  1.0f, 0.0f, /* bottom right */
    };
    // clang-format on

    rub->uploadStaticBuffer(vbuf_.get(), vs);

    float color[4] = {
        static_cast<float>(subtitle_.color[0]) / 255.0f,
        static_cast<float>(subtitle_.color[1]) / 255.0f,
        static_cast<float>(subtitle_.color[2]) / 255.0f,
        static_cast<float>(0xff - subtitle_.color[3]) / 255.0f,
    };
    rub->updateDynamicBuffer(ubuf_.get(), 128, 16, color);

    subtitle_slots_[rhi_->currentFrameSlot()] = subtitle_;
    QRhiTextureSubresourceUploadDescription desc(subtitle_.buffer.get(),
                                                 static_cast<qint32>(subtitle_.size));
    desc.setDataStride(subtitle_.stride);
    rub->uploadTexture(texture_.get(), QRhiTextureUploadDescription{ { 0, 0, desc } });
}

void SubtitleRenderItem::draw(QRhiCommandBuffer *cb, const QRhiViewport& viewport)
{
    std::scoped_lock lock(mtx_);

    cb->setGraphicsPipeline(pipeline_.get());
    cb->setViewport(viewport);
    cb->setShaderResources(srb_.get());

    const QRhiCommandBuffer::VertexInput input{ vbuf_.get(), 0 };
    cb->setVertexInput(0, 1, &input);
    cb->draw(4);
}