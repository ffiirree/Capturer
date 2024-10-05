#include "texture-widget-rhi.h"

#include "logging.h"

TextureRhiWidget::TextureRhiWidget(QWidget *parent)
    : QRhiWidget(parent)
{
    items_.emplace_back(std::make_shared<ImageRenderItem>());

    connect(this, &TextureRhiWidget::updateRequest, this, [this] { update(); }, Qt::QueuedConnection);
}

AVPixelFormat TextureRhiWidget::format(const AVPixelFormat expected, const AVPixelFormat dft)
{
    for (const auto& fmt : av::texture_formats()) {
        if (fmt == expected) {
            return expected;
        }
    }

    return dft;
}

void TextureRhiWidget::initialize(QRhiCommandBuffer *)
{
    if (rhi_ != rhi()) {
        rhi_ = rhi();
    }
}

void TextureRhiWidget::render(QRhiCommandBuffer *cb)
{
    //    if (!rhi_ || !cb) return;

    std::scoped_lock lock(mtx_);

    const auto rub = rhi_->nextResourceUpdateBatch();

    const auto rtsz = renderTarget()->pixelSize();
    render_sz_      = image_sz_.scaled(rtsz, Qt::KeepAspectRatio);

    items_slots_[rhi_->currentFrameSlot()] = items_;
    for (const auto& item : items_) {
        const auto scaled = item->size().scaled(rtsz, Qt::KeepAspectRatio);

        float scale_x = (static_cast<float>(scaled.width()) / static_cast<float>(rtsz.width())) * hflip_;
        float scale_y = (static_cast<float>(scaled.height()) / static_cast<float>(rtsz.height())) * vflip_;

        item->create(rhi_, renderTarget());
        item->upload(rub, scale_x, scale_y);
    }

    cb->beginPass(renderTarget(), Qt::black, { 1.0f, 0 }, rub);

    for (const auto& item : items_) {
        item->draw(cb, { 0, 0, static_cast<float>(rtsz.width()), static_cast<float>(rtsz.height()) });
    }

    cb->endPass();
}

void TextureRhiWidget::present(const av::frame& frame)
{
    std::scoped_lock lock(mtx_);

    if (items_.front()->attach(frame)) {
        image_sz_ = QSize{ frame->width, frame->height };
        emit updateRequest();
    }
}

void TextureRhiWidget::hdr(bool en)
{
    items_.front()->hdr(en);
    emit updateRequest();
}

void TextureRhiWidget::present(const std::list<Subtitle>& subtitles, int changed)
{
    if (!changed) return;

    std::scoped_lock lock(mtx_);

    items_.erase(items_.begin() + 1, items_.end());
    for (const auto& subtitle : subtitles) {
        const auto item = std::make_shared<SubtitleRenderItem>();
        item->attach(subtitle);
        items_.emplace_back(item);
    }
}