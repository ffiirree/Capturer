#include "circlecursor.h"
#include <QPainter>
#include <QDebug>

CircleCursor::CircleCursor()
    : CircleCursor(3)
{ }


CircleCursor::CircleCursor(int width)
    : width_(width)
{
    repaint();
}


void CircleCursor::repaint()
{
    auto rect = cursor_.rect();
    cursor_.fill(Qt::transparent);
    auto center = rect.center();

    QPainter painter(&cursor_);

    painter.setPen(QPen(QColor("#888888"), 2, Qt::SolidLine));
    painter.setBrush(QColor(225, 200, 0, 175));
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.drawEllipse(center.x() - width_/2, center.y() - width_/2, width_, width_);

    painter.end();
}
