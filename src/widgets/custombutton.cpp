#include "custombutton.h"
#include <QKeyEvent>

CustomButton::CustomButton(QWidget *parent)
    : QAbstractButton (parent)
{
    connect(this, &CustomButton::toggled, [this](){ update(); });
}

CustomButton::CustomButton(const QSize& size, bool checkable, QWidget *parent)
    : CustomButton(parent)
{
    setFixedSize(size);
    setCheckable(checkable);
}

void CustomButton::enterEvent(QEvent *)
{
    hover_ = true;
}

void CustomButton::leaveEvent(QEvent *)
{
    hover_ = false;
}

void CustomButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), backgroundColor());

    paint(&painter);

    if (!isEnabled())
        painter.fillRect(rect(), QColor{ 125, 125, 125, 75 });
}