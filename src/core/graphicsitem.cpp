#include "graphicsitem.h"

#include "logging.h"
#include "resizer.h"

#include <numbers>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>

static QVector<QPointF> compute_arrow_vertexes(QPointF begin, QPointF end)
{
    QVector<QPointF> points(6, QPointF{});

    double slopy = std::atan2(end.y() - begin.y(), end.x() - begin.x());

    double par1 = 20.0;
    double par2 = 25.0;

    double alpha1 = 13.0 * std::numbers::pi / 180.0;
    double alpha2 = 26.0 * std::numbers::pi / 180.0;

    points[0] = begin;

    points[1] = { end.x() - par1 * std::cos(alpha1 + slopy), end.y() - par1 * std::sin(alpha1 + slopy) };
    points[5] = { end.x() - par1 * std::cos(alpha1 - slopy), end.y() + par1 * std::sin(alpha1 - slopy) };

    points[2] = { end.x() - par2 * std::cos(alpha2 + slopy), end.y() - par2 * std::sin(alpha2 + slopy) };
    points[4] = { end.x() - par2 * std::cos(alpha2 - slopy), end.y() + par2 * std::sin(alpha2 - slopy) };

    points[3] = end;

    return points;
}

inline std::ostream& operator<<(std::ostream& out, const QRectF& rect)
{
    return out << "<<" << rect.left() << ", " << rect.top() << ">, <" << rect.width() << " x "
               << rect.height() << ">>";
}

GraphicsItem::GraphicsItem(graph_t graph, const QVector<QPointF>& vs)
    : graph_(graph),
      vertexes_(vs),
      QGraphicsItem()
{
    setFlag(QGraphicsItem::ItemIsFocusable);
    setFlag(QGraphicsItem::ItemIsMovable);
    setFlag(QGraphicsItem::ItemIsSelectable);
    setAcceptHoverEvents(true);
}

QRectF GraphicsItem::boundingRect() const
{
    return QRectF(vertexes_[0] - QPointF{ 2, 2 }, vertexes_[1] + QPointF{ 2, 2 });
}

void GraphicsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    painter->setPen(QPen{ Qt::red, 3, Qt::SolidLine });

    painter->setRenderHint(QPainter::Antialiasing, true);

    // painter->setBrush(Qt::red);
    switch (graph_) {
    case graph_t::rectangle: painter->drawRect({ vertexes_[0], vertexes_[1] }); break;
    case graph_t::ellipse: painter->drawEllipse({ vertexes_[0], vertexes_[1] }); break;
    case graph_t::arrow:
        painter->setBrush(painter->pen().color());
        painter->drawPolygon(compute_arrow_vertexes(vertexes_[0], vertexes_[1]));
        break;
    case graph_t::line: painter->drawLine({ vertexes_[0], vertexes_[1] }); break;
    case graph_t::curve:
    case graph_t::mosaic:
    case graph_t::eraser: painter->drawPolyline(vertexes_);
    case graph_t::text: break;
    default: break;
    }

    //
    painter->setRenderHint(QPainter::Antialiasing, false);

    painter->save();
    auto color = QColor("#969696");
    switch (graph_) {
    case graph_t::rectangle:
    case graph_t::ellipse:
        painter->setBrush(Qt::NoBrush);

        // box border
        painter->setPen(QPen(color, 1, Qt::DashLine));
        painter->drawRect({ vertexes_[0], vertexes_[1] });

        // anchors
        painter->setBrush(Qt::white);
        painter->setPen(QPen(color, 1, Qt::SolidLine));
        painter->drawRects(Resizer{ static_cast<int>(vertexes_[0].x()), static_cast<int>(vertexes_[0].y()),
                                    static_cast<int>(vertexes_[1].x()), static_cast<int>(vertexes_[1].y()) }
                               .anchors());
        break;

    case graph_t::line:
    case graph_t::arrow: {
        painter->setPen(QPen(color, 1, Qt::DashLine));
        painter->drawLine({ vertexes_[0], vertexes_[1] });

        painter->setPen(QPen(color, 1, Qt::SolidLine));
        painter->setBrush(Qt::white);
        Resizer resizer{ static_cast<int>(vertexes_[0].x()), static_cast<int>(vertexes_[0].y()),
                         static_cast<int>(vertexes_[1].x()), static_cast<int>(vertexes_[1].y()) };
        painter->drawRect(resizer.X1Y1Anchor());
        painter->drawRect(resizer.X2Y2Anchor());
        break;
    }

    default: break;
    }

    painter->restore();
}

void GraphicsItem::pushVertex(const QPointF& v)
{
    switch (graph_) {
    case graph_t::rectangle:
    case graph_t::ellipse:
    case graph_t::arrow:
    case graph_t::line: vertexes_.back() = v; break;
    case graph_t::curve:
    case graph_t::mosaic:
    case graph_t::eraser: vertexes_.push_back(v); break;
    case graph_t::text: break;
    }
    prepareGeometryChange();
    update();
}

void GraphicsItem::focusInEvent(QFocusEvent *)
{
    update();
    LOG(INFO) << vertexes_[0].x() << ": focusInEvent: " << boundingRect();
}

void GraphicsItem::focusOutEvent(QFocusEvent *)
{
    update();
    LOG(INFO) << vertexes_[0].x() << ": focusOutEvent: " << boundingRect();
}

void GraphicsItem::hoverEnterEvent(QGraphicsSceneHoverEvent *)
{
    LOG(INFO) << "hoverEnterEvent: " << boundingRect();
}

void GraphicsItem::hoverMoveEvent(QGraphicsSceneHoverEvent *)
{
    // LOG(INFO) << "hoverMoveEvent: " << boundingRect();
}

void GraphicsItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *)
{
    LOG(INFO) << "hoverLeaveEvent: " << boundingRect();
}

void GraphicsItem::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete) {
        setVisible(false);
    }
    LOG(INFO) << vertexes_[0].x() << ": keyPressEvent";
}

void GraphicsItem::keyReleaseEvent(QKeyEvent *) { LOG(INFO) << vertexes_[0].x() << ": keyReleaseEvent"; }

void GraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    spos_ = event->pos();

    LOG(INFO) << vertexes_[0].x() << ": mousePressEvent";

    QGraphicsItem::mousePressEvent(event);
}

void GraphicsItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    auto offset = event->pos() - spos_;
    // moveBy(offset.x(), offset.y());

    LOG(INFO) << vertexes_[0].x() << ": mouseMoveEvent: (" << boundingRect().x() << ", "
              << boundingRect().y() << "), " << boundingRect().width() << "x" << boundingRect().height();

    QGraphicsItem::mouseMoveEvent(event);
}

void GraphicsItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    LOG(INFO) << vertexes_[0].x() << ": mouseReleaseEvent";
    QGraphicsItem::mouseReleaseEvent(event);
}