#include "linewidthwidget.h"
#include <QPainter>
#include <QWheelEvent>

void WidthButton::wheelEvent(QWheelEvent *event)
{
    if (!isChecked()) return;

    width_ +=  event->angleDelta().y() / 120;
    checkValue();

    emit changed(width_);

    event->accept();

    update();
}

void WidthButton::paint(QPainter * painter)
{
    painter->setPen(iconColor());
    painter->setBrush(iconColor());
    painter->setRenderHint(QPainter::Antialiasing, true);

    auto c = rect().center();
    painter->drawEllipse(c.x() - width_/2, c.y() - width_/2, width_, width_);
}
