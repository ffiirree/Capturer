#include "linewidthwidget.h"
#include <QPainter>
#include <QHBoxLayout>
#include <QWheelEvent>
#include <QDebug>

void WidthButton::wheelEvent(QWheelEvent *event)
{
    width_ +=  event->delta() / 120;
    checkValue();

    emit changed(width_);

    event->accept();

    update();
}

void WidthButton::paint(QPainter * painter)
{
    painter->setPen(icon_color_);
    painter->setBrush(icon_color_);
    painter->setRenderHint(QPainter::Antialiasing, true);

    auto c = rect().center();
    painter->drawEllipse(c.x() - width_/2, c.y() - width_/2, width_, width_);
}
