#include "slider.h"

#include <QMouseEvent>
#include <QStyle>

Slider::Slider(QWidget *parent)
    : QSlider(parent)
{}

Slider::Slider(Qt::Orientation orientation, QWidget *parent)
    : QSlider(orientation, parent)
{}

void Slider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        setSliderDown(true);

        const auto to = std::chrono::milliseconds{ QStyle::sliderValueFromPosition(
            minimum(), maximum(), event->pos().x(), width()) };
        emit seek(to,
                  std::clamp<std::chrono::nanoseconds>(to - std::chrono::milliseconds(value()), -10s, 10s));
        setValue(static_cast<int>(to.count()));
    }
    else {
        QSlider::mousePressEvent(event);
    }
}

void Slider::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        const auto to = std::chrono::milliseconds{ QStyle::sliderValueFromPosition(
            minimum(), maximum(), event->pos().x(), width()) };
        emit seek(to,
                  std::clamp<std::chrono::nanoseconds>(to - std::chrono::milliseconds(value()), -10s, 10s));
        setValue(static_cast<int>(to.count()));
    }
    else {
        QSlider::mouseMoveEvent(event);
    }
}

void Slider::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        setSliderDown(false);
    }
    else {
        QSlider::mouseReleaseEvent(event);
    }
}
