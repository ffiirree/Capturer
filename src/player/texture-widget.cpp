#include "texture-widget.h"

#include <QPainter>

TextureWidget::TextureWidget(QWidget *parent)
    : QWidget(parent)
{}

void TextureWidget::present(AVFrame *frame)
{
    if (!frame) return;

    std::lock_guard lock(mtx_);

    frame_ = QPixmap::fromImage(QImage{ static_cast<const uchar *>(frame->data[0]), frame->width,
                                        frame->height, QImage::Format_RGB888 });

    update();
}

void TextureWidget::present(const QPixmap& pixmap)
{
    std::lock_guard lock(mtx_);
    frame_ = pixmap;

    update();
}

void TextureWidget::present(const QImage& image)
{
    std::lock_guard lock(mtx_);
    frame_ = QPixmap::fromImage(image);

    update();
}

// TODO: D3D11, OpenGL ...
void TextureWidget::paintEvent(QPaintEvent *)
{
    if (std::lock_guard lock(mtx_); !frame_.isNull()) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);

        auto winrect = rect();

        auto imgsize = frame_.size().scaled(winrect.width(), winrect.height(), Qt::KeepAspectRatio);
        auto imgrect = QRect{ { 0, 0 }, imgsize };

        imgrect.moveCenter(winrect.center());

        painter.drawPixmap(imgrect, frame_);
    }
}
