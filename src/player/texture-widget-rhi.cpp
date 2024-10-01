#include "texture-widget-rhi.h"

#include "logging.h"

#include <QFile>
#include <utility>

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

static QMatrix4x4 BT601_FULL_CM {
    1.0000f,  0.0000f,  1.77200f, -0.88600f,
    1.0000f, -0.1646f, -0.57135f,  0.36795f,
    1.0000f,  1.4200f,  0.00000f, -0.71000f,
    0.0000f,  0.0000f,  0.00000f,  1.00000f
};

static QMatrix4x4 BT601_TV_CM {
    1.164f,  0.000f,  1.596f, -0.8708f,
    1.164f, -0.392f, -0.813f,  0.5296f,
    1.164f,  2.017f,  0.000f, -1.0810f,
    0.000f,  0.000f,  0.000f,  1.0000f
};

static QMatrix4x4 BT709_FULL_CM {
    1.0000f,  0.000000f,  1.57480f,  -0.790488f,
    1.0000f, -0.187324f, -0.468124f,  0.329010f,
    1.0000f,  1.855600f,  0.00000f,  -0.931439f,
    0.0000f,  0.000000f,  0.00000f,   1.000000f
};

static QMatrix4x4 BT709_TV_CM {
    1.1644f,  0.0000f,  1.7927f, -0.9729f,
    1.1644f, -0.2132f, -0.5329f,  0.3015f,
    1.1644f,  2.1124f,  0.0000f, -1.1334f,
    0.0000f,  0.0000f,  0.0000f,  1.0000f
};

static QMatrix4x4 BT2020_FULL_CM {
    1.0000f,  0.0000f,  1.4746f, -0.7402f,
    1.0000f, -0.1646f, -0.5714f,  0.3694f,
    1.0000f,  1.8814f,  0.0000f, -0.9445f,
    0.0000f,  0.0000f,  0.0000f,  1.0000f
};

static QMatrix4x4 BT2020_TV_CM {
    1.1644f,  0.000f,   1.6787f, -0.9157f,
    1.1644f, -0.1874f, -0.6504f,  0.3475f,
    1.1644f,  2.1418f,  0.0000f, -1.1483f,
    0.0000f,  0.0000f,  0.0000f,  1.0000f
};

static QMatrix4x4 IDENTITY_CM {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};
// clang-format on

static QMatrix4x4 GetColorMatrix(AVColorSpace space, AVColorRange range)
{
    switch (space) {
    case AVCOL_SPC_RGB:         return IDENTITY_CM;
    case AVCOL_SPC_BT470BG:     return (range == AVCOL_RANGE_JPEG) ? BT601_FULL_CM : BT601_TV_CM;
    case AVCOL_SPC_BT709:
    case AVCOL_SPC_UNSPECIFIED: return (range == AVCOL_RANGE_JPEG) ? BT709_FULL_CM : BT709_TV_CM;
    case AVCOL_SPC_BT2020_NCL:  return (range == AVCOL_RANGE_JPEG) ? BT2020_FULL_CM : BT2020_TV_CM;
    default:                    loge("unsupported color-space: {}", av::to_string(space)); return IDENTITY_CM;
    }
}

static QShader GetShaderByName(const QString& name)
{
    QFile f(name);
    return f.open(QIODevice::ReadOnly) ? QShader::fromSerialized(f.readAll()) : QShader();
}

static std::vector<TextureDescription> GetTextureParams(AVPixelFormat fmt)
{
    switch (fmt) {
    case AV_PIX_FMT_YUYV422: return { { 2, 1, QRhiTexture::RGBA8, 4 } };

    case AV_PIX_FMT_GRAY8:   return { { 1, 1, QRhiTexture::R8, 1 } };

    case AV_PIX_FMT_YUV420P:
        return {
            { 1, 1, QRhiTexture::R8, 1 },
            { 2, 2, QRhiTexture::R8, 1 },
            { 2, 2, QRhiTexture::R8, 1 },
        };

    case AV_PIX_FMT_YUV422P:
        return {
            { 1, 1, QRhiTexture::R8, 1 },
            { 2, 1, QRhiTexture::R8, 1 },
            { 2, 1, QRhiTexture::R8, 1 },
        };

    case AV_PIX_FMT_YUV444P:
        return {
            { 1, 1, QRhiTexture::R8, 1 },
            { 1, 1, QRhiTexture::R8, 1 },
            { 1, 1, QRhiTexture::R8, 1 },
        };

    case AV_PIX_FMT_YUV410P:
        return {
            { 1, 1, QRhiTexture::R8, 1 },
            { 4, 4, QRhiTexture::R8, 1 },
            { 4, 4, QRhiTexture::R8, 1 },
        };

    case AV_PIX_FMT_YUV411P:
        return {
            { 1, 1, QRhiTexture::R8, 1 },
            { 4, 1, QRhiTexture::R8, 1 },
            { 4, 1, QRhiTexture::R8, 1 },
        };

    case AV_PIX_FMT_GRAY16LE: return { { 1, 1, QRhiTexture::R16, 2 } };

    case AV_PIX_FMT_YUVJ420P:
        return {
            { 1, 1, QRhiTexture::R8, 1 },
            { 2, 2, QRhiTexture::R8, 1 },
            { 2, 2, QRhiTexture::R8, 1 },
        };

    case AV_PIX_FMT_YUVJ422P:
        return {
            { 1, 1, QRhiTexture::R8, 1 },
            { 2, 1, QRhiTexture::R8, 1 },
            { 2, 1, QRhiTexture::R8, 1 },
        };

    case AV_PIX_FMT_YUVJ444P:
        return {
            { 1, 1, QRhiTexture::R8, 1 },
            { 1, 1, QRhiTexture::R8, 1 },
            { 1, 1, QRhiTexture::R8, 1 },
        };

    case AV_PIX_FMT_UYVY422: return { { 2, 1, QRhiTexture::RGBA8, 4 } };

    case AV_PIX_FMT_YUVA444P:
        return {
            { 1, 1, QRhiTexture::R8, 1 },
            { 1, 1, QRhiTexture::R8, 1 },
            { 1, 1, QRhiTexture::R8, 1 },
            { 1, 1, QRhiTexture::R8, 1 },
        };

    case AV_PIX_FMT_NV12:
    case AV_PIX_FMT_NV21:
        return {
            { 1, 1, QRhiTexture::R8, 1 },
            { 2, 2, QRhiTexture::RG8, 2 },
        };

    case AV_PIX_FMT_ARGB:
    case AV_PIX_FMT_RGBA:
    case AV_PIX_FMT_ABGR:
    case AV_PIX_FMT_BGRA:
    case AV_PIX_FMT_0RGB:
    case AV_PIX_FMT_RGB0:
    case AV_PIX_FMT_0BGR:
    case AV_PIX_FMT_BGR0: return { { 1, 1, QRhiTexture::RGBA8, 4 } };

    case AV_PIX_FMT_YUV420P10LE:
        return {
            { 1, 1, QRhiTexture::R16, 2 },
            { 2, 2, QRhiTexture::R16, 2 },
            { 2, 2, QRhiTexture::R16, 2 },
        };

    case AV_PIX_FMT_GBRP:
        return {
            { 1, 1, QRhiTexture::R8, 1 },
            { 1, 1, QRhiTexture::R8, 1 },
            { 1, 1, QRhiTexture::R8, 1 },
        };

    default: loge("unsupported pixel format: {}", av::to_string(fmt)); return {};
    }
}

TextureRhiWidget::TextureRhiWidget(QWidget *parent)
    : QRhiWidget(parent)
{
    connect(this, &TextureRhiWidget::updateRequest, this, [this] { update(); }, Qt::QueuedConnection);
}

std::vector<AVPixelFormat> TextureRhiWidget::PixelFormats()
{
    // clang-format off
    return {
        AV_PIX_FMT_YUV420P, ///< planar YUV 4:2:0, 12bpp, (1 Cr & Cb sample per 2x2 Y samples)

        AV_PIX_FMT_YUYV422, ///< packed YUV 4:2:2, 16bpp, Y0 Cb Y1 Cr

        AV_PIX_FMT_YUV422P, ///< planar YUV 4:2:2, 16bpp, (1 Cr & Cb sample per 2x1 Y samples)
        AV_PIX_FMT_YUV444P, ///< planar YUV 4:4:4, 24bpp, (1 Cr & Cb sample per 1x1 Y samples)
        AV_PIX_FMT_YUV410P, ///< planar YUV 4:1:0,  9bpp, (1 Cr & Cb sample per 4x4 Y samples)
        AV_PIX_FMT_YUV411P, ///< planar YUV 4:1:1, 12bpp, (1 Cr & Cb sample per 4x1 Y samples)

        AV_PIX_FMT_GRAY8,     ///<        Y        ,  8bpp
        AV_PIX_FMT_GRAY16LE,  ///<        Y        , 16bpp, little-endian

        AV_PIX_FMT_YUVJ420P,  ///< planar YUV 4:2:0, 12bpp, full scale (JPEG), deprecated in favor of AV_PIX_FMT_YUV420P and setting color_range
        AV_PIX_FMT_YUVJ422P,  ///< planar YUV 4:2:2, 16bpp, full scale (JPEG), deprecated in favor of AV_PIX_FMT_YUV422P and setting color_range
        AV_PIX_FMT_YUVJ444P,  ///< planar YUV 4:4:4, 24bpp, full scale (JPEG), deprecated in favor of AV_PIX_FMT_YUV444P and setting color_range

        AV_PIX_FMT_YUVA444P,  ///< planar YUV 4:4:4 32bpp, (1 Cr & Cb sample per 1x1 Y & A samples)

        AV_PIX_FMT_NV12,    ///< planar YUV 4:2:0, 12bpp, 1 plane for Y and 1 plane for the UV components, < which are interleaved (first byte U and the following byte V)
        AV_PIX_FMT_NV21,    ///< as above, but U and V bytes are swapped

        AV_PIX_FMT_ARGB,    ///< packed ARGB 8:8:8:8, 32bpp, ARGBARGB...
        AV_PIX_FMT_RGBA,    ///< packed RGBA 8:8:8:8, 32bpp, RGBARGBA...
        AV_PIX_FMT_ABGR,    ///< packed ABGR 8:8:8:8, 32bpp, ABGRABGR...
        AV_PIX_FMT_BGRA,    ///< packed BGRA 8:8:8:8, 32bpp, BGRABGRA...

        AV_PIX_FMT_0RGB,    ///< packed RGB 8:8:8, 32bpp, XRGBXRGB...   X=unused/undefined
        AV_PIX_FMT_RGB0,    ///< packed RGB 8:8:8, 32bpp, RGBXRGBX...   X=unused/undefined
        AV_PIX_FMT_0BGR,    ///< packed BGR 8:8:8, 32bpp, XBGRXBGR...   X=unused/undefined
        AV_PIX_FMT_BGR0,    ///< packed BGR 8:8:8, 32bpp, BGRXBGRX...   X=unused/undefined

        AV_PIX_FMT_YUV420P10LE, ///< planar YUV 4:2:0, 15bpp, (1 Cr & Cb sample per 2x2 Y samples), little-endian

        AV_PIX_FMT_GBRP,      ///< planar GBR 4:4:4 24bpp
    };
    // clang-format on
}

bool TextureRhiWidget::IsSupported(const AVPixelFormat pix_fmt)
{
    for (const auto& fmt : PixelFormats()) {
        if (fmt == pix_fmt) return true;
    }
    return false;
}

static QString ShaderNameForPixelFormat(AVPixelFormat fmt)
{
    switch (fmt) {
    case AV_PIX_FMT_GRAY8:
    case AV_PIX_FMT_GRAY16LE:    return "y";
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUV422P:
    case AV_PIX_FMT_YUV444P:
    case AV_PIX_FMT_YUV410P:
    case AV_PIX_FMT_YUV411P:
    case AV_PIX_FMT_YUVJ420P:
    case AV_PIX_FMT_YUVJ422P:
    case AV_PIX_FMT_YUVJ444P:    return "yuv";
    case AV_PIX_FMT_YUYV422:     return "yuyv";
    case AV_PIX_FMT_UYVY422:     return "uyvy";
    case AV_PIX_FMT_YUVA444P:    return "ayuv_planar";
    case AV_PIX_FMT_NV12:        return "nv12";
    case AV_PIX_FMT_NV21:        return "nv21";
    case AV_PIX_FMT_ARGB:
    case AV_PIX_FMT_0RGB:        return "argb";
    case AV_PIX_FMT_RGBA:
    case AV_PIX_FMT_RGB0:        return "bgra";
    case AV_PIX_FMT_ABGR:
    case AV_PIX_FMT_0BGR:        return "abgr";
    case AV_PIX_FMT_BGRA:
    case AV_PIX_FMT_BGR0:        return "bgra";
    case AV_PIX_FMT_YUV420P10LE: return "yuv_p10le";
    case AV_PIX_FMT_GBRP:        return "gbr_planar";
    default:                     return {};
    }
}

void TextureRhiWidget::UpdateTextures(QRhiResourceUpdateBatch *rub)
{
    auto params = GetTextureParams(fmt_.pix_fmt);

    if (format_changed_) {
        format_changed_ = false;

        logd("[    TEXTURE] update textures : {}({}, {})", av::to_string(fmt_.pix_fmt),
             av::to_string(fmt_.color.space), av::to_string(fmt_.color.range));

        for (size_t i = 0; i < params.size(); ++i) {
            planes_[i].reset(rhi_->newTexture(
                params[i].format, { fmt_.width / params[i].xfactor, fmt_.height / params[i].yfactor }));
            planes_[i]->create();
        }

        QRhiShaderResourceBinding bindings[5];
        bindings[0] = QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            ubuf_.get());
        for (size_t i = 0; i < params.size(); ++i) {
            bindings[i + 1] = QRhiShaderResourceBinding::sampledTexture(
                i + 1, QRhiShaderResourceBinding::FragmentStage, planes_[i].get(), sampler_.get());
        }
        srb_->setBindings(bindings, bindings + params.size() + 1);
        srb_->create();

        SetupPipeline(pipeline_.get(), srb_.get(), ShaderNameForPixelFormat(fmt_.pix_fmt));
    }

    for (size_t i = 0; i < params.size(); ++i) {
        QRhiTextureSubresourceUploadDescription desc{};
        desc.setData(QByteArray::fromRawData(reinterpret_cast<const char *>(frame_->data[i]),
                                             (fmt_.width * fmt_.height * params[i].bytes) /
                                                 (params[i].xfactor * params[i].yfactor)));
        desc.setDataStride(frame_->linesize[i]);

        rub->uploadTexture(planes_[i].get(), QRhiTextureUploadDescription{ { 0, 0, desc } });
    }
}

void TextureRhiWidget::SetupPipeline(QRhiGraphicsPipeline *pipeline, QRhiShaderResourceBindings *bindings,
                                     const QString& frag)
{
    const auto name = QString(":/src/resources/shaders/") + frag + ".frag.qsb";

    pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    pipeline->setShaderStages({
        { QRhiShaderStage::Vertex, GetShaderByName(":/src/resources/shaders/vertex.vert.qsb") },
        { QRhiShaderStage::Fragment, GetShaderByName(name) },
    });

    QRhiVertexInputLayout layout{};
    layout.setBindings({ 4 * sizeof(float) });
    layout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float) },
    });

    pipeline->setVertexInputLayout(layout);
    pipeline->setShaderResourceBindings(bindings);
    pipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    pipeline->create();
}

void TextureRhiWidget::initialize(QRhiCommandBuffer *cb)
{
    if (rhi_ != rhi()) {
        pipeline_.reset();
        rhi_ = rhi();
    }

    if (!pipeline_) {
        vbuf_.reset(rhi_->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertices)));
        vbuf_->create();

        ubuf_.reset(rhi_->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64 + 64 + 16));
        ubuf_->create();

        sampler_.reset(rhi_->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        sampler_->create();

        srb_.reset(rhi_->newShaderResourceBindings());
        pipeline_.reset(rhi_->newGraphicsPipeline());

        auto rub = rhi_->nextResourceUpdateBatch();
        rub->uploadStaticBuffer(vbuf_.get(), vertices);
        cb->resourceUpdate(rub);
    }
}

void TextureRhiWidget::render(QRhiCommandBuffer *cb)
{
    std::scoped_lock lock(mtx_, sub_mtx_);

    if (!frame_ || frame_->width <= 0 || frame_->height <= 0) return;

    auto rub = rhi_->nextResourceUpdateBatch();

    // keep the video frames alive until we know that they are not needed anymore
    frame_slots_[rhi_->currentFrameSlot()] = frame_;

    UpdateTextures(rub);

    mvp_.setToIdentity();
    pixel_size_    = renderTarget()->pixelSize();
    const auto fsz = QSize{ fmt_.width, fmt_.height }.scaled(pixel_size_, Qt::KeepAspectRatio);
    mvp_.scale(float(fsz.width()) / pixel_size_.width(), float(fsz.height()) / pixel_size_.height());
    rub->updateDynamicBuffer(ubuf_.get(), 0, 64, mvp_.constData());
    rub->updateDynamicBuffer(ubuf_.get(), 64, 64,
                             GetColorMatrix(fmt_.color.space, fmt_.color.range).constData());
    for (auto& item : items_) {
        rub->updateDynamicBuffer(item.ubuf.get(), 0, 64, mvp_.constData());
    }

    if (sub_rub_) {
        rub->merge(sub_rub_);
        sub_rub_->release();
        sub_rub_ = nullptr;
    }

    cb->beginPass(renderTarget(), Qt::black, { 1.0f, 0 }, rub);

    // frame
    cb->setGraphicsPipeline(pipeline_.get());
    cb->setViewport(QRhiViewport(0, 0, static_cast<float>(pixel_size_.width()),
                                 static_cast<float>(pixel_size_.height())));
    cb->setShaderResources(srb_.get());
    const QRhiCommandBuffer::VertexInput vbb{ vbuf_.get(), 0 };
    cb->setVertexInput(0, 1, &vbb);
    cb->draw(4);

    // keep the subtitle resources alive until they are not needed
    subtitle_slots_[rhi_->currentFrameSlot()] = subtitles_;
    items_slots_[rhi_->currentFrameSlot()]    = items_;
    for (auto& item : items_) {
        cb->setGraphicsPipeline(item.pipeline.get());
        cb->setShaderResources(item.srb.get());

        const QRhiCommandBuffer::VertexInput svbb{ item.vbuf.get(), 0 };
        cb->setVertexInput(0, 1, &svbb);
        cb->draw(4);
    }

    cb->endPass();
}

void TextureRhiWidget::present(const av::frame& frame)
{
    if (!frame || !frame->data[0] || frame->width <= 0 || frame->height <= 0 ||
        frame->format == AV_PIX_FMT_NONE) {
        return;
    }

    std::lock_guard lock(mtx_);
    frame_ = frame;

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

        format_changed_ = true;
    }

    emit updateRequest();
}

void TextureRhiWidget::present(const std::list<Subtitle>& subtitles, int changed)
{
    if (!changed || (subtitles.empty() && subtitles_.empty()) || !rhi_) return;

    logd("[RHI-TEXTURE] [S] changed {}, subtitles {}", changed, subtitles.size());

    std::lock_guard lock(sub_mtx_);

    subtitles_ = subtitles;

    if (sub_rub_) {
        sub_rub_->release();
        sub_rub_ = nullptr;
    }
    items_.clear();

    if (subtitles.empty()) return;

    sub_rub_ = rhi_->nextResourceUpdateBatch();

    for (auto image : subtitles) {
        RenderItem item{};
        item.vbuf.reset(rhi_->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertices)));
        item.vbuf->create();

        const auto x = image.x.get<float>();
        const auto y = image.y.get<float>();
        const auto w = image.w.get<float>();
        const auto h = image.h.get<float>();

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
        sub_rub_->uploadStaticBuffer(item.vbuf.get(), vs);

        item.ubuf.reset(rhi_->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64 + 64 + 16));
        item.ubuf->create();

        float color[4] = {
            static_cast<float>(image.color[0]) / 255.0f,
            static_cast<float>(image.color[1]) / 255.0f,
            static_cast<float>(image.color[2]) / 255.0f,
            static_cast<float>(0xff - image.color[3]) / 255.0f,
        };
        sub_rub_->updateDynamicBuffer(item.ubuf.get(), 128, 16, color);

        item.tex.reset(
            rhi_->newTexture((image.format == AV_PIX_FMT_PAL8) ? QRhiTexture::R8 : QRhiTexture::RGBA8,
                             { image.w.num, image.h.num }));
        item.tex->create();
        QRhiTextureSubresourceUploadDescription desc{};
        desc.setData(QByteArray::fromRawData(reinterpret_cast<const char *>(image.buffer.get()),
                                             image.w.num * image.h.num * 4));
        desc.setDataStride(image.stride);
        sub_rub_->uploadTexture(item.tex.get(), QRhiTextureUploadDescription{ { 0, 0, desc } });

        item.srb.reset(rhi_->newShaderResourceBindings());
        QRhiShaderResourceBinding bindings[2];
        bindings[0] = QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            item.ubuf.get());
        bindings[1] = QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage,
                                                                item.tex.get(), sampler_.get());
        item.srb->setBindings(bindings, bindings + 2);
        item.srb->create();

        item.pipeline.reset(rhi_->newGraphicsPipeline());
        QRhiGraphicsPipeline::TargetBlend blend{
            .enable   = true,
            .srcColor = QRhiGraphicsPipeline::SrcAlpha,
            .dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha,
        };
        item.pipeline->setTargetBlends({ blend });
        item.pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);

        SetupPipeline(item.pipeline.get(), item.srb.get(),
                      (image.format == AV_PIX_FMT_PAL8) ? "ass" : "rgba");

        items_.push_back(std::move(item));
    }
}