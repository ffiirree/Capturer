#include "widthbutton.h"

#include <cmath>
#include <QStyle>
#include <QWheelEvent>

void WidthButton::setValue(int width, bool silence)
{
    if (width_ == width) return;

    width_ = std::clamp(width, min_, max_);
    if (!silence) emit changed(width_);

    auto __width = std::lround((width_ / static_cast<float>(max_)) * 8);
    if (__attr_width != __width && __width >= 1) {
        setProperty("width", QVariant::fromValue(__width));
        style()->polish(this);
    }
}

void WidthButton::wheelEvent(QWheelEvent *event)
{
    if (!isChecked()) return;

    setValue(width_ + event->angleDelta().y() / 120, false);
}
