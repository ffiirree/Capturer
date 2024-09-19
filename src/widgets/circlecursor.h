#ifndef CAPTURER_CIRCLE_CURSOR_H
#define CAPTURER_CIRCLE_CURSOR_H

#include <QObject>
#include <QPen>
#include <QPixmap>

namespace cursor
{
    QPixmap circle(qreal width, const QPen& pen = QPen(QColor(0x88, 0x88, 0x88), 2),
                   const QBrush& brush = QColor(225, 200, 0, 175));
}

#endif //! CAPTURER_CIRCLE_CURSOR_H
