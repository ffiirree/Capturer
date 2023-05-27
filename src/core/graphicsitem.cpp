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
    int margin = 4;
    if (graph_ == graph_t::arrow) margin = 30;

    return QRectF(QPointF(std::min(vertexes_[0].x(), vertexes_[1].x()),
                          std::min(vertexes_[0].y(), vertexes_[1].y())),
                  QPointF(std::max(vertexes_[0].x(), vertexes_[1].x()),
                          std::max(vertexes_[0].y(), vertexes_[1].y())))
        .adjusted(-margin, -margin, margin, margin);
}

void GraphicsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->save();

    pen_.setCapStyle(Qt::RoundCap);
    pen_.setJoinStyle(Qt::RoundJoin);

    painter->setPen(pen_);
    painter->setBrush(brush_);

    painter->setRenderHint(QPainter::Antialiasing, true);

    // painter->setBrush(Qt::red);
    switch (graph_) {
    case graph_t::rectangle: painter->drawRect({ vertexes_[0], vertexes_[1] }); break;
    case graph_t::ellipse: painter->drawEllipse({ vertexes_[0], vertexes_[1] }); break;
    case graph_t::arrow:
        painter->setPen(QPen(pen_.color(), 1, Qt::SolidLine, Qt::SquareCap, Qt::BevelJoin));
        painter->setBrush(pen_.color());
        painter->drawPolygon(compute_arrow_vertexes(vertexes_[0], vertexes_[1]));
        break;
    case graph_t::line: painter->drawLine({ vertexes_[0], vertexes_[1] }); break;
    case graph_t::curve:
    case graph_t::mosaic:
    case graph_t::eraser: painter->drawPolyline(vertexes_);
    default: break;
    }
    painter->restore();

    //
    if (hasFocus()) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, false);

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
            painter->drawRects(
                Resizer{ static_cast<int>(vertexes_[0].x()), static_cast<int>(vertexes_[0].y()),
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
}

void GraphicsItem::pushVertex(const QPointF& v)
{
    switch (graph_) {
    case graph_t::rectangle:
    case graph_t::ellipse:
    case graph_t::arrow:
    case graph_t::line: vertexes_[1] = v; break;
    case graph_t::curve:
    case graph_t::mosaic:
    case graph_t::eraser: vertexes_.push_back(v); break;
    case graph_t::text: break;
    }
    prepareGeometryChange();
    update();
}

bool GraphicsItem::invalid()
{
    switch (graph_) {
    case graph_t::rectangle:
    case graph_t::ellipse:
    case graph_t::arrow:
    case graph_t::line:
        if (vertexes_.size() >= 2) {
            auto offset = vertexes_[1] - vertexes_[0];
            return std::abs(offset.x()) < 4 && std::abs(offset.y()) < 4;
        }
        return true;
    case graph_t::curve:
    case graph_t::mosaic:
    case graph_t::eraser: return false;
    default: return true;
    }
}

Resizer::PointPosition GraphicsItem::location(const QPoint& pos)
{
    Resizer::PointPosition location = Resizer::DEFAULT;

    switch (graph_) {
    case graph_t::rectangle: return Resizer({ vertexes_[0], vertexes_[1] }).absolutePos(pos);
    case graph_t::ellipse: {
        QRect rect{ vertexes_[0].toPoint(), vertexes_[1].toPoint() };

        QRegion r1(rect.adjusted(-2, -2, 2, 2), QRegion::Ellipse);
        QRegion r2(rect.adjusted(2, 2, -2, -2), QRegion::Ellipse);

        auto hover_pos = Resizer({ vertexes_[0], vertexes_[1] }).absolutePos(pos);
        if (!(hover_pos & Resizer::ADJUST_AREA) && r1.contains(pos) && !r2.contains(pos)) {
            return Resizer::BORDER;
        }

        return hover_pos;
    }
    case graph_t::line:
    case graph_t::arrow: {
        Resizer resizer({ vertexes_[0], vertexes_[1] });

        auto x1y1_anchor = resizer.X1Y1Anchor();
        auto x2y2_anchor = resizer.X2Y2Anchor();
        QPolygon polygon;
        polygon.push_back(x1y1_anchor.topLeft());
        polygon.push_back(x1y1_anchor.bottomRight());
        polygon.push_back(x2y2_anchor.bottomRight());
        polygon.push_back(x2y2_anchor.topLeft());

        if (x1y1_anchor.contains(pos)) {
            return Resizer::X1Y1_ANCHOR;
        }
        else if (x2y2_anchor.contains(pos)) {
            return Resizer::X2Y2_ANCHOR;
        }
        else if (polygon.containsPoint(pos, Qt::OddEvenFill)) {
            return Resizer::BORDER;
        }
        return Resizer::DEFAULT;
    }
    default: return Resizer::DEFAULT;
    }
}

void GraphicsItem::focusInEvent(QFocusEvent *event)
{
    update();
    QGraphicsItem::focusInEvent(event);
}

void GraphicsItem::focusOutEvent(QFocusEvent *event)
{
    update();
    QGraphicsItem::focusOutEvent(event);
}

void GraphicsItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    QGraphicsItem::hoverEnterEvent(event);
}

void GraphicsItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    if (status_ == NORMAL) {
        auto hpos = location(event->pos().toPoint());

        if (hpos != hover_location_ && onhovered) onhovered(hpos);

        hover_location_ = hpos;
    }
    QGraphicsItem::hoverMoveEvent(event);
}

void GraphicsItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    hover_location_ = Resizer::OUTSIDE;
    onhovered(Resizer::OUTSIDE);

    QGraphicsItem::hoverLeaveEvent(event);
}

void GraphicsItem::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete) {
        setVisible(false);
        if (ondeleted) ondeleted();
    }
    QGraphicsItem::keyPressEvent(event);
}

void GraphicsItem::keyReleaseEvent(QKeyEvent *event) { QGraphicsItem::keyReleaseEvent(event); }

void GraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (status_ == SHIELDED) return;

    location_ = location(event->pos().toPoint());
    if (location_ & Resizer::ANCHOR) {
        status_ = RESIZING;
        spos_   = event->pos();
    }
    else if (location_ & Resizer::BORDER) {
        status_ = MOVING;
        QGraphicsItem::mousePressEvent(event);
    }
}

void GraphicsItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    auto cursor_pos = event->pos().toPoint();

    switch (status_) {
    case MOVING: QGraphicsItem::mouseMoveEvent(event); break;
    case RESIZING: {
        Resizer resizer(vertexes_[0], vertexes_[1]);
        // clang-format off
        switch (location_) {
        case Resizer::X1Y1_ANCHOR:  resizer.x1(cursor_pos.x()); resizer.y1(cursor_pos.y()); break;
        case Resizer::X2Y1_ANCHOR:  resizer.x2(cursor_pos.x()); resizer.y1(cursor_pos.y()); break;
        case Resizer::X1Y2_ANCHOR:  resizer.x1(cursor_pos.x()); resizer.y2(cursor_pos.y()); break;
        case Resizer::X2Y2_ANCHOR:  resizer.x2(cursor_pos.x()); resizer.y2(cursor_pos.y()); break;
        case Resizer::X1_ANCHOR:    resizer.x1(cursor_pos.x()); break;
        case Resizer::X2_ANCHOR:    resizer.x2(cursor_pos.x()); break;
        case Resizer::Y1_ANCHOR:    resizer.y1(cursor_pos.y()); break;
        case Resizer::Y2_ANCHOR:    resizer.y2(cursor_pos.y()); break;
        default: break;
        }
        // clang-format on
        vertexes_[0] = resizer.point1();
        vertexes_[1] = resizer.point2();

        prepareGeometryChange();

        update();

        break;
    }
    default: break;
    }
}

void GraphicsItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (status_ == MOVING || status_ == RESIZING) {
        status_ = NORMAL;
    }
    QGraphicsItem::mouseReleaseEvent(event);
}