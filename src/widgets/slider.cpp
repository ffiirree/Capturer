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

        int64_t to = QStyle::sliderValueFromPosition(minimum(), maximum(), event->pos().x(), width());
        emit seek(to * 1'000, std::clamp<int64_t>((to - static_cast<int64_t>(value())) * 1'000, -10'000'000,
                                                  10'000'000));
        setValue(static_cast<int>(to));
    }
    else {
        QSlider::mousePressEvent(event);
    }
}

void Slider::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        int64_t to = QStyle::sliderValueFromPosition(minimum(), maximum(), event->pos().x(), width());
        emit seek(to * 1'000, std::clamp<int64_t>((to - static_cast<int64_t>(value())) * 1'000, -10'000'000,
                                                  10'000'000));
        setValue(static_cast<int>(to));
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
