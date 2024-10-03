#include "image-render-item.h"

#include "logging.h"
#include "texture-helper.h"

extern "C" {
#include <libavutil/imgutils.h>
}

// clang-format off
// normalized device coordinates
static constexpr float vertices[] = {
  // vertex coordinate: [-1.0, 1.0]    / texture coordinate: [0.0, 1.0]
  //  x      y                         / x     y
    -1.0f, -1.0f,  /* bottom left  */  0.0f, 1.0f, /* top    left  */
    +1.0f, -1.0f,  /* bottom right */  1.0f, 1.0f, /* top    right */
    -1.0f, +1.0f,  /* top    left  */  0.0f, 0.0f, /* bottom left  */
    +1.0f, +1.0f,  /* top    right */  1.0f, 0.0f, /* bottom right */
};
// clang-format on

bool ImageRenderItem::attach(const std::any& attachment)
{
    try {
        const auto frame = std::any_cast<av::frame>(attachment);
        if (!frame || !frame->data[0] || frame->width <= 0 || frame->height <= 0 ||
            frame->format == AV_PIX_FMT_NONE) {
            return false;
        }

        std::scoped_lock lock(mtx_);
        frame_    = frame;
        uploaded_ = false;

        if ((fmt_.width != frame_->width || fmt_.height != frame_->height) ||
            (fmt_.pix_fmt != frame_->format) ||
            (fmt_.color.space != frame_->colorspace || fmt_.color.range != frame_->color_range)) {

            fmt_.width               = frame_->width;
            fmt_.height              = frame_->height;
            fmt_.pix_fmt             = static_cast<AVPixelFormat>(frame_->format);
            fmt_.sample_aspect_ratio = frame_->sample_aspect_ratio;

            fmt_.color = {
                frame_->colorspace,
                frame_->color_range,
                frame_->color_primaries,
                frame_->color_trc,
            };

            created_ = false;
        }
    }
    catch (const std::bad_any_cast& e) {
        loge("failed to cast image: {}", e.what());
        return false;
    }

    return true;
}

void ImageRenderItem::create(QRhi *rhi, QRhiRenderTarget *rt)
{
    if (created_.exchange(true)) return;
    std::scoped_lock lock(mtx_);

    rhi_ = rhi;

    vbuf_.reset(rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertices)));
    vbuf_->create();

    ubuf_.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64 + 64 + 16));
    ubuf_->create();

    sampler_.reset(rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                   QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
    sampler_->create();

    std::vector<QRhiShaderResourceBinding> bindings{};
    bindings.emplace_back(QRhiShaderResourceBinding::uniformBuffer(
        0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, ubuf_.get()));

    auto params = av::get_texture_desc(fmt_.pix_fmt);
    planes_.clear();
    for (auto& param : params) {
        const auto texture =
            rhi->newTexture(param.format, { fmt_.width / param.scale_x, fmt_.height / param.scale_y });
        texture->create();
        planes_.emplace_back(texture);
    }

    for (size_t i = 0; i < params.size(); ++i) {
        bindings.emplace_back(QRhiShaderResourceBinding::sampledTexture(
            static_cast<int>(i + 1), QRhiShaderResourceBinding::FragmentStage, planes_[i].get(),
            sampler_.get()));
    }
    srb_.reset(rhi->newShaderResourceBindings());
    srb_->setBindings(bindings.begin(), bindings.end());
    srb_->create();

    pipeline_.reset(rhi->newGraphicsPipeline());
    pipeline_->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    pipeline_->setShaderStages({
        { QRhiShaderStage::Vertex, av::get_shader(":/src/resources/shaders/vertex.vert.qsb") },
        { QRhiShaderStage::Fragment, av::get_frag_shader(fmt_.pix_fmt) },
    });

    QRhiVertexInputLayout layout{};
    layout.setBindings({ 4 * sizeof(float) });
    layout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float) },
    });

    pipeline_->setVertexInputLayout(layout);
    pipeline_->setShaderResourceBindings(srb_.get());
    pipeline_->setRenderPassDescriptor(rt->renderPassDescriptor());
    pipeline_->create();
}

void ImageRenderItem::upload(QRhiResourceUpdateBatch *rub, float scale_x, float scale_y)
{
    std::scoped_lock lock(mtx_);

    mvp_.setToIdentity();
    mvp_.scale(scale_x, scale_y);
    rub->updateDynamicBuffer(ubuf_.get(), 0, 64, mvp_.constData());

    if (uploaded_.exchange(true)) return;

    rub->uploadStaticBuffer(vbuf_.get(), vertices);

    rub->updateDynamicBuffer(ubuf_.get(), 64, 64, av::get_color_matrix_coefficients(fmt_));

    const auto params                      = av::get_texture_desc(fmt_.pix_fmt);
    frame_slots_[rhi_->currentFrameSlot()] = frame_;
    for (size_t i = 0; i < params.size(); ++i) {
        QRhiTextureSubresourceUploadDescription desc(frame_->data[i], frame_->linesize[i] * frame_->height /
                                                                          params[i].scale_y);
        desc.setDataStride(frame_->linesize[i]);
        rub->uploadTexture(planes_[i].get(), QRhiTextureUploadDescription{ { 0, 0, desc } });
    }
}

void ImageRenderItem::draw(QRhiCommandBuffer *cb, const QRhiViewport& viewport)
{
    std::scoped_lock lock(mtx_);

    cb->setGraphicsPipeline(pipeline_.get());
    cb->setViewport(viewport);
    cb->setShaderResources(srb_.get());

    const QRhiCommandBuffer::VertexInput input{ vbuf_.get(), 0 };
    cb->setVertexInput(0, 1, &input);
    cb->draw(4);
}