#include "circlecursor.h"

#include <QPainter>

QPixmap cursor::circle(qreal width, const QPen& pen, const QBrush& brush)
{
    width = std::min<qreal>(width, 71);

    QPixmap cursor{ 75, 75 };
    cursor.fill(Qt::transparent);

    QPainter painter(&cursor);

    painter.setPen(pen);
    painter.setBrush(brush);

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.drawEllipse(QPointF{ cursor.rect().center() }, width / 2, width / 2);

    return cursor;
}
