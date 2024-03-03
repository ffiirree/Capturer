#include "slider.h"

#include <QMouseEvent>
#include <QStyle>

using namespace std::chrono;

Slider::Slider(QWidget *parent)
    : QSlider(parent)
{}

Slider::Slider(const Qt::Orientation orientation, QWidget *parent)
    : QSlider(orientation, parent)
{
    setCursor(Qt::PointingHandCursor);
}

void Slider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        setSliderDown(true);

        const int ts = QStyle::sliderValueFromPosition(minimum(), maximum(), event->pos().x(), width());

        emit seek(milliseconds{ ts }, std::clamp<milliseconds>(milliseconds{ ts - value() }, -10s, 10s));

        setValue(ts);
    }
    else {
        QSlider::mousePressEvent(event);
    }
}

void Slider::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        const int ts = QStyle::sliderValueFromPosition(minimum(), maximum(), event->pos().x(), width());

        emit seek(milliseconds{ ts }, std::clamp<milliseconds>(milliseconds{ ts - value() }, -10s, 10s));

        setValue(ts);
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
