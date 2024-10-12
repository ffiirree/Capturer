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
    // 90
    -1.0f, -1.0f,  /* bottom left  */  1.0f, 1.0f, /* top    right */
    +1.0f, -1.0f,  /* bottom right */  1.0f, 0.0f, /* bottom right */
    -1.0f, +1.0f,  /* top    left  */  0.0f, 1.0f, /* top    left  */
    +1.0f, +1.0f,  /* top    right */  0.0f, 0.0f, /* bottom left  */
    // 180
    -1.0f, -1.0f,  /* bottom left  */  1.0f, 0.0f, /* bottom right */
    +1.0f, -1.0f,  /* bottom right */  0.0f, 0.0f, /* bottom left  */
    -1.0f, +1.0f,  /* top    left  */  1.0f, 1.0f, /* top    right */
    +1.0f, +1.0f,  /* top    right */  0.0f, 1.0f, /* top    left  */
    // 270
    -1.0f, -1.0f,  /* bottom left  */  0.0f, 0.0f, /* bottom left  */
    +1.0f, -1.0f,  /* bottom right */  0.0f, 1.0f, /* top    left  */
    -1.0f, +1.0f,  /* top    left  */  1.0f, 0.0f, /* bottom right */
    +1.0f, +1.0f,  /* top    right */  1.0f, 1.0f, /* top    right */
};
// clang-format on

av::vformat_t get_video_format(const av::frame& frame)
{
    auto sw_format = static_cast<AVPixelFormat>(frame->format);
    auto hwaccel   = AV_HWDEVICE_TYPE_NONE;
    if (frame->hw_frames_ctx) {
        const auto frames_ctx = reinterpret_cast<AVHWFramesContext *>(frame->hw_frames_ctx->data);
        sw_format             = frames_ctx->sw_format;
        hwaccel               = frames_ctx->device_ctx->type;
    }

    return av::vformat_t{
        .width               = frame->width,
        .height              = frame->height,
        .pix_fmt             = static_cast<AVPixelFormat>(frame->format),
        .sample_aspect_ratio = frame->sample_aspect_ratio,
        .color =
            av::vformat_t::color_t{
                .space     = frame->colorspace,
                .range     = frame->color_range,
                .primaries = frame->color_primaries,
                .transfer  = frame->color_trc,
            },
        .hwaccel    = hwaccel,
        .sw_pix_fmt = sw_format,
    };
}

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

        const auto vfmt = get_video_format(frame);
        if (fmt_ != vfmt) {
            fmt_ = vfmt;

            created_ = false;
        }
    }
    catch (const std::bad_any_cast& e) {
        loge("failed to cast image: {}", e.what());
        return false;
    }

    return true;
}

QSize ImageRenderItem::size() const
{
    return frame_ ? ((rotation_ / 90) % 2 ? QSize{ frame_->height, frame_->width }
                                          : QSize{ frame_->width, frame_->height })
                  : QSize{};
}

void ImageRenderItem::hdr(bool en)
{
    if (hdr_ != en) {
        uploaded_ = false;
        created_  = false;
    }

    hdr_ = en;
}

void ImageRenderItem::rotate(int angle)
{
    if (rotation_ != angle) {
        rotation_ = angle;
        uploaded_ = false;
    }
}

void ImageRenderItem::create(QRhi *rhi, QRhiRenderTarget *rt)
{
    if (created_.exchange(true)) return;
    std::scoped_lock lock(mtx_);

    rhi_ = rhi;

#ifdef _WIN32
    converter_ = std::make_unique<D3D11TextureConverter>(rhi);
#endif

    vbuf_.reset(rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertices)));
    vbuf_->create();

    ubuf_.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64 + 64 + 16));
    ubuf_->create();

    sampler_.reset(rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                   QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
    sampler_->create();

    srb_.reset(rhi->newShaderResourceBindings());

    pipeline_.reset(rhi->newGraphicsPipeline());
    pipeline_->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    pipeline_->setShaderStages({
        { QRhiShaderStage::Vertex, av::get_shader(":/src/resources/shaders/vertex.vert.qsb") },
        { QRhiShaderStage::Fragment, av::get_frag_shader(fmt_, hdr_) },
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

    if (!frame_ || frame_->width <= 0 || frame_->height <= 0 || !frame_->data[0]) return;
    if (!uploaded_.exchange(true)) {
        std::vector<QRhiShaderResourceBinding> bindings{};
        bindings.emplace_back(QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            ubuf_.get()));
        auto params = av::get_texture_desc(fmt_.sw_pix_fmt);
        planes_.clear();

        if (fmt_.hwaccel != AV_HWDEVICE_TYPE_NONE) {
            const auto tex = converter_->texture(frame_);

            for (auto& param : params) {
                const auto texture = rhi_->newTexture(
                    param.format, { fmt_.width / param.scale_x, fmt_.height / param.scale_y });
                texture->createFrom({ tex, 0 });
                planes_.emplace_back(texture);
            }
        }
        else {
            for (auto& param : params) {
                const auto texture = rhi_->newTexture(
                    param.format, { fmt_.width / param.scale_x, fmt_.height / param.scale_y });
                texture->create();
                planes_.emplace_back(texture);
            }
        }

        for (size_t i = 0; i < params.size(); ++i) {
            bindings.emplace_back(QRhiShaderResourceBinding::sampledTexture(
                static_cast<int>(i + 1), QRhiShaderResourceBinding::FragmentStage, planes_[i].get(),
                sampler_.get()));
        }
        srb_->setBindings(bindings.begin(), bindings.end());
        srb_->create();

        if (fmt_.hwaccel == AV_HWDEVICE_TYPE_NONE) {
            frame_slots_[rhi_->currentFrameSlot()] = frame_;
            for (size_t i = 0; i < params.size(); ++i) {
                QRhiTextureSubresourceUploadDescription desc(
                    frame_->data[i], frame_->linesize[i] * frame_->height / params[i].scale_y);
                desc.setDataStride(frame_->linesize[i]);
                rub->uploadTexture(planes_[i].get(), QRhiTextureUploadDescription{ { 0, 0, desc } });
            }
        }

        rub->uploadStaticBuffer(vbuf_.get(), vertices);
        rub->updateDynamicBuffer(ubuf_.get(), 64, 64, av::get_color_matrix_coefficients(fmt_));
    }

    mvp_.setToIdentity();
    mvp_.scale(scale_x, scale_y);
    rub->updateDynamicBuffer(ubuf_.get(), 0, 64, mvp_.constData());
}

void ImageRenderItem::draw(QRhiCommandBuffer *cb, const QRhiViewport& viewport)
{
    std::scoped_lock lock(mtx_);

    cb->setGraphicsPipeline(pipeline_.get());
    cb->setViewport(viewport);
    cb->setShaderResources(srb_.get());

    const QRhiCommandBuffer::VertexInput input{ vbuf_.get(), 64 * (rotation_ / 90) };
    cb->setVertexInput(0, 1, &input);
    cb->draw(4);
}