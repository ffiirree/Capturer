#include "widthbutton.h"

#include <cmath>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>
#include <QWheelEvent>

WidthButton::WidthButton(bool checkable, QWidget *parent)
    : QCheckBox(parent)
{
    setCheckable(checkable);
}

void WidthButton::setValue(int value, bool silence)
{
    if (width_ == value) return;

    width_ = std::clamp(value, 1, MAX());
    if (!silence) emit changed(width_);

    update();
}

void WidthButton::wheelEvent(QWheelEvent *event)
{
    if (!isChecked()) return;

    setValue(width_ + event->angleDelta().y() / 120, false);
}

void WidthButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QStyleOptionButton opt{};
    initStyleOption(&opt);
    style()->drawControl(QStyle::CE_CheckBox, &opt, &painter, this);

    const auto r = QRectF{ rect() };
    painter.setPen(isChecked() ? Qt::white : opt.palette.color(QPalette::Text));
    painter.setBrush(isChecked() ? Qt::white : opt.palette.color(QPalette::Text));
    const auto w = std::min<qreal>(width_ / 3.0, MAX() / 3.0);
    painter.drawEllipse(r.center(), w, w);
}