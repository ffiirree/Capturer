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
    for (const auto& fmt : ImageRenderItem::formats()) {
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

    const float rt_w = static_cast<float>(renderTarget()->pixelSize().width());
    const float rt_h = static_cast<float>(renderTarget()->pixelSize().height());
    render_sz_       = image_sz_.scaled(renderTarget()->pixelSize(), Qt::KeepAspectRatio);
    float scale_x    = (static_cast<float>(render_sz_.width()) / rt_w) * hflip_;
    float scale_y    = (static_cast<float>(render_sz_.height()) / rt_h) * vflip_;

    items_slots_[rhi_->currentFrameSlot()] = items_;
    for (const auto& item : items_) {
        item->create(rhi_, renderTarget());
        item->upload(rub, scale_x, scale_y);
    }

    cb->beginPass(renderTarget(), Qt::black, { 1.0f, 0 }, rub);

    for (const auto& item : items_) {
        item->draw(cb, { 0, 0, rt_w, rt_h });
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

void TextureRhiWidget::present(const std::list<Subtitle>& subtitles, int changed)
{
    if (!changed || subtitles.empty()) return;

    std::scoped_lock lock(mtx_);

    items_.erase(items_.begin() + 1, items_.end());
    for (const auto& subtitle : subtitles) {
        const auto item = std::make_shared<SubtitleRenderItem>();
        item->attach(subtitle);
        items_.emplace_back(item);
    }
}