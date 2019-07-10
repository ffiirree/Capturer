#include "command.h"
#include <cmath>

void PaintCommand::getArrowPoints(QPoint begin, QPoint end, QPoint* points)
{
    double par = 10.0;
    double par2 = 27.0;
    double slopy = atan2((end.y() - begin.y()), (end.x() - begin.x()));
    double alpha = 30 * 3.14 / 180;

    points[1] = QPoint(end.x() - int(par * cos(alpha + slopy)) - int(9 * cos(slopy)), end.y() - int(par*sin(alpha + slopy)) - int(9 * sin(slopy)));
    points[5] = QPoint(end.x() - int(par * cos(alpha - slopy)) - int(9 * cos(slopy)), end.y() + int(par*sin(alpha - slopy)) - int(9 * sin(slopy)));

    points[2] = QPoint(end.x() - int(par2 * cos(alpha + slopy)), end.y() - int(par2*sin(alpha + slopy)));
    points[4] = QPoint(end.x() - int(par2 * cos(alpha - slopy)), end.y() + int(par2*sin(alpha - slopy)));

    points[0] = begin;
    points[3] = end;
}

void PaintCommand::execute(QPainter *painter_)
{
    painter_->save();
    painter_->setPen(pen());
    if(isFill()) painter_->setBrush(pen().color());

    switch (graph()) {
    case RECTANGLE:
        painter_->drawRect(QRect(points()[0], points()[1]));
        break;

    case ELLIPSE:
        painter_->setRenderHint(QPainter::Antialiasing, true);
        painter_->drawEllipse(QRect(points()[0], points()[1]));
        break;

    case LINE:
        painter_->setRenderHint(QPainter::Antialiasing, true);
        painter_->drawLine(points()[0], points()[1]);
        break;

    case ARROW:
    {
        painter_->setRenderHint(QPainter::Antialiasing, true);
        QPoint ps[6];
        getArrowPoints(points()[0], points()[1], ps);
        painter_->setBrush(pen().color());
        painter_->drawPolygon(ps, 6);

        break;
    }

    case CURVES:
    case ERASER:
    case MOSAIC:
        painter_->setRenderHint(QPainter::Antialiasing, true);
        painter_->drawPolyline(points());
        break;

    case TEXT:
    {
        auto edit = reinterpret_cast<TextEdit*>(widget());
        painter_->setFont(edit->font());
        painter_->drawText(edit->geometry().adjusted(2, 2, 0, 0), Qt::TextWordWrap, edit->toPlainText());

        break;
    }

    default: break;
    }
    painter_->restore();
}
