#include "graphicsitems.h"

#include "logging.h"

#include <cmath>
#include <numbers>
#include <QAbstractTextDocumentLayout>
#include <QDebug>
#include <QGraphicsScene>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

GraphicsItem::GraphicsItem(QGraphicsItem *parent)
    : QGraphicsItem(parent)
{
    setFlag(QGraphicsItem::ItemIsFocusable);
    setFlag(QGraphicsItem::ItemIsMovable);
    // setFlag(QGraphicsItem::ItemIsSelectable);
    setAcceptHoverEvents(true);
}

void GraphicsItem::focusInEvent(QFocusEvent *event)
{
    if (event->reason() == Qt::MouseFocusReason && (hover_location_ != ResizerLocation::EMPTY_INSIDE))
        onfocus();

    QGraphicsItem::focusInEvent(event);
}

void GraphicsItem::focusOutEvent(QFocusEvent *event)
{
    onblur();
    QGraphicsItem::focusOutEvent(event);
}

void GraphicsItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    auto hpos = location(event->pos());

    if (hpos != hover_location_) {
        onhover(hpos);
        hover_location_ = hpos;
    }

    QGraphicsItem::hoverMoveEvent(event);
}

void GraphicsItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    onhover(ResizerLocation::DEFAULT);
    hover_location_ = ResizerLocation::DEFAULT;

    QGraphicsItem::hoverLeaveEvent(event);
}

void GraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (!!(hover_location_ & ResizerLocation::ANCHOR)) {
            adjusting_       = canvas::adjusting_t::resizing;
            before_resizing_ = geometry_;
        }
        else if (!!(hover_location_ & ResizerLocation::BORDER) ||
                 !!(hover_location_ & ResizerLocation::FILLED_INSIDE)) {
            adjusting_     = canvas::adjusting_t::moving;
            before_moving_ = pos();
        }
        else if (!!(hover_location_ & ResizerLocation::ROTATE_ANCHOR)) {
            adjusting_       = canvas::adjusting_t::rotating;
            mpos_            = event->scenePos();
            before_rotating_ = angle_;
        }
    }

    (hover_location_ != ResizerLocation::EMPTY_INSIDE) ? QGraphicsItem::mouseReleaseEvent(event)
                                                       : event->ignore();
}

void GraphicsItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    auto cpos = event->pos().toPoint();

    switch (adjusting_) {
    case canvas::adjusting_t::moving: QGraphicsItem::mouseMoveEvent(event); break;
    case canvas::adjusting_t::resizing: {
        ResizerF resizer(geometry_);
        // clang-format off
        switch (hover_location_) {
        case ResizerLocation::X1Y1_ANCHOR:  resizer.x1(cpos.x()); resizer.y1(cpos.y()); break;
        case ResizerLocation::X2Y1_ANCHOR:  resizer.x2(cpos.x()); resizer.y1(cpos.y()); break;
        case ResizerLocation::X1Y2_ANCHOR:  resizer.x1(cpos.x()); resizer.y2(cpos.y()); break;
        case ResizerLocation::X2Y2_ANCHOR:  resizer.x2(cpos.x()); resizer.y2(cpos.y()); break;
        case ResizerLocation::X1_ANCHOR:    resizer.x1(cpos.x()); break;
        case ResizerLocation::X2_ANCHOR:    resizer.x2(cpos.x()); break;
        case ResizerLocation::Y1_ANCHOR:    resizer.y1(cpos.y()); break;
        case ResizerLocation::Y2_ANCHOR:    resizer.y2(cpos.y()); break;
        default: break;
        }
        // clang-format on

        resize(resizer, hover_location_);

        break;
    }
    default: break;
    }
}

void GraphicsItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    switch (adjusting_) {
    case canvas::adjusting_t::moving: onmove(before_moving_); break;
    case canvas::adjusting_t::resizing: onresize(before_resizing_, hover_location_); break;
    case canvas::adjusting_t::rotating: onrotate(before_rotating_); break;

    default: break;
    }

    adjusting_       = canvas::adjusting_t::none;
    before_resizing_ = {};
    before_moving_   = {};
    mpos_            = {};
    before_rotating_ = {};

    (hover_location_ != ResizerLocation::EMPTY_INSIDE) ? QGraphicsItem::mouseReleaseEvent(event)
                                                       : event->ignore();
}

///

static QPainterPath shape_from_path(const QPainterPath& path, const QPen& pen)
{
    // We unfortunately need this hack as QPainterPathStroker will set a width of 1.0
    // if we pass a value of 0.0 to QPainterPathStroker::setWidth()
    const qreal penWidthZero = qreal(0.00000001);

    if (path == QPainterPath() || pen == Qt::NoPen) return path;
    QPainterPathStroker ps;
    ps.setCapStyle(pen.capStyle());
    if (pen.widthF() <= 0.0)
        ps.setWidth(penWidthZero);
    else
        ps.setWidth(pen.widthF());
    ps.setJoinStyle(pen.joinStyle());
    ps.setMiterLimit(pen.miterLimit());
    QPainterPath p = ps.createStroke(path);
    p.addPath(path);
    return p;
}

///

GraphicsLineItem::GraphicsLineItem(QGraphicsItem *parent)
    : GraphicsItem(parent)
{}

GraphicsLineItem::GraphicsLineItem(const QPointF& x1, const QPointF& x2, QGraphicsItem *parent)
    : GraphicsItem(parent)
{
    setVertexes(x1, x2);
}

void GraphicsLineItem::setVertexes(const QPointF& p1, const QPointF& p2)
{
    if (geometry_.point1() == p1 && geometry_.point2() == p2) return;

    prepareGeometryChange();
    geometry_.coords(p1, p2);
    update();
}

void GraphicsLineItem::push(const QPointF& point)
{
    if (geometry_.point2() == point) return;

    prepareGeometryChange();
    geometry_.coords(geometry_.point1(), point);
    update();
}

QPen GraphicsLineItem::pen() const { return pen_; }

void GraphicsLineItem::setPen(const QPen& pen)
{
    if (pen_ == pen) return;

    prepareGeometryChange();
    pen_ = pen;
    update();
}

QRectF GraphicsLineItem::boundingRect() const { return shape().controlPointRect(); }

QPainterPath GraphicsLineItem::shape() const
{
    QPainterPath path;
    if (geometry_ == ResizerF{}) return path;

    path.moveTo(geometry_.point1());
    path.lineTo(geometry_.point2());

    QPen _pen(pen_);
    _pen.setWidth(std::max(pen_.widthF(), geometry_.borderWidth()));
    return shape_from_path(path, _pen);
}

ResizerLocation GraphicsLineItem::location(const QPointF& p) const
{
    if (geometry_.isX1Y1Anchor(p)) {
        return ResizerLocation::X1Y1_ANCHOR;
    }
    else if (geometry_.isX2Y2Anchor(p)) {
        return ResizerLocation::X2Y2_ANCHOR;
    }
    else if (shape().contains(p)) {
        return ResizerLocation::BORDER;
    }

    return ResizerLocation::OUTSIDE;
}

void GraphicsLineItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    if (invalid()) return;

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    painter->setPen(pen_);
    painter->drawLine(geometry_.point1(), geometry_.point2());

    if (option->state & (QStyle::State_Selected | QStyle::State_HasFocus)) {
        auto color = QColor("#969696");

        painter->setPen(QPen(color, 1, Qt::DashLine));
        painter->drawLine(geometry_.point1(), geometry_.point2());

        painter->setPen(QPen(color, 1.5, Qt::SolidLine));
        painter->setBrush(Qt::white);
        painter->drawEllipse(geometry_.X1Y1Anchor());
        painter->drawEllipse(geometry_.X2Y2Anchor());
    }
}

void GraphicsLineItem::resize(const ResizerF& g, ResizerLocation)
{
    prepareGeometryChange();

    geometry_.coords(g.point1(), g.point2());

    update();
}

QGraphicsItem *GraphicsLineItem::copy() const
{
    auto item  = new GraphicsLineItem(geometry_.point1(), geometry_.point2());
    item->pen_ = pen();

    return item;
}

///
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

///

GraphicsArrowItem::GraphicsArrowItem(QGraphicsItem *parent)
    : GraphicsItem(parent)
{}

GraphicsArrowItem::GraphicsArrowItem(const QPointF& x1, const QPointF& x2, QGraphicsItem *parent)
    : GraphicsItem(parent)
{
    setVertexes(x1, x2);
}

void GraphicsArrowItem::setVertexes(const QPointF& p1, const QPointF& p2)
{
    if (geometry_.point1() == p1 && geometry_.point2() == p2) return;

    prepareGeometryChange();
    geometry_.coords(p1, p2);
    polygon_ = compute_arrow_vertexes(p1, p2);
    update();
}

void GraphicsArrowItem::push(const QPointF& point)
{
    if (geometry_.point2() == point) return;

    prepareGeometryChange();
    geometry_.point2(point);
    polygon_ = compute_arrow_vertexes(geometry_.point1(), geometry_.point2());
    update();
}

QPen GraphicsArrowItem::pen() const { return pen_; }

void GraphicsArrowItem::setPen(const QPen& pen)
{
    if (pen_ == pen) return;

    prepareGeometryChange();
    pen_ = pen;
    update();
}

QRectF GraphicsArrowItem::boundingRect() const { return shape().controlPointRect(); }

QPainterPath GraphicsArrowItem::shape() const
{
    QPainterPath path;
    path.addPolygon(polygon_);
    return shape_from_path(path, { pen_.color(), geometry_.borderWidth(), Qt::SolidLine });
}

ResizerLocation GraphicsArrowItem::location(const QPointF& p) const
{
    if (geometry_.isX1Y1Anchor(p)) {
        return ResizerLocation::X1Y1_ANCHOR;
    }
    else if (geometry_.isX2Y2Anchor(p)) {
        return ResizerLocation::X2Y2_ANCHOR;
    }
    else if (shape().contains(p)) {
        return ResizerLocation::BORDER;
    }

    return ResizerLocation::OUTSIDE;
}

void GraphicsArrowItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    if (invalid()) return;

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    painter->setPen({ pen_.color(), 1, Qt::SolidLine });
    painter->setBrush(pen_.color());
    painter->drawPolygon(polygon_);

    if (option->state & (QStyle::State_Selected | QStyle::State_HasFocus)) {
        auto color = QColor("#969696");

        painter->setPen(QPen(color, 1, Qt::DashLine));
        painter->drawLine(polygon_[0], polygon_[3]);

        painter->setPen(QPen(color, 1.5, Qt::SolidLine));
        painter->setBrush(Qt::white);
        ResizerF resizer{ polygon_[0], polygon_[3] };
        painter->drawEllipse(resizer.X1Y1Anchor());
        painter->drawEllipse(resizer.X2Y2Anchor());
    }
}

void GraphicsArrowItem::resize(const ResizerF& g, ResizerLocation)
{
    prepareGeometryChange();

    geometry_.coords(g.point1(), g.point2());
    polygon_ = compute_arrow_vertexes(geometry_.point1(), geometry_.point2());

    update();
}

QGraphicsItem *GraphicsArrowItem::copy() const
{
    auto item      = new GraphicsArrowItem();
    item->polygon_ = polygon_;
    item->pen_     = pen();
    return item;
}

///

GraphicsRectItem::GraphicsRectItem(QGraphicsItem *parent)
    : GraphicsItem(parent)
{}

GraphicsRectItem::GraphicsRectItem(const QPointF& x1, const QPointF& x2, QGraphicsItem *parent)
    : GraphicsItem(parent)
{
    setVertexes(x1, x2);
}

void GraphicsRectItem::setVertexes(const QPointF& p1, const QPointF& p2)
{
    if (geometry_.point1() == p1 && geometry_.point2() == p2) return;

    prepareGeometryChange();
    geometry_.coords(p1, p2);
    update();
}

void GraphicsRectItem::push(const QPointF& point)
{
    if (geometry_.point2() == point) return;
    prepareGeometryChange();
    geometry_.point2(point);
    update();
}

QRectF GraphicsRectItem::boundingRect() const { return shape().controlPointRect(); }

QPainterPath GraphicsRectItem::shape() const
{
    QPainterPath path;
    path.addRect(geometry_.boundingRect());
    return path;
}

ResizerLocation GraphicsRectItem::location(const QPointF& p) const
{
    ResizerLocation l = geometry_.absolutePos(p);
    if (l == ResizerLocation::EMPTY_INSIDE && filled()) return ResizerLocation::BORDER;
    return l;
}

QPen GraphicsRectItem::pen() const { return pen_; }

void GraphicsRectItem::setPen(const QPen& pen)
{
    if (pen_ == pen) return;

    prepareGeometryChange();
    pen_ = pen;
    update();
}

QBrush GraphicsRectItem::brush() const { return brush_; }

void GraphicsRectItem::setBrush(const QBrush& brush)
{
    if (brush_ == brush) return;

    brush_ = brush;
    update();
}

bool GraphicsRectItem::filled() const { return filled_; }

void GraphicsRectItem::fill(bool f)
{
    if (filled_ == f) return;

    filled_ = f;
    update();
}

void GraphicsRectItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    if (invalid()) return;

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    pen_.setCapStyle(Qt::RoundCap);
    pen_.setJoinStyle(Qt::RoundJoin);

    painter->setPen(pen_);
    auto _brush = filled() ? ((brush_ == Qt::NoBrush) ? pen_.color() : brush_) : Qt::NoBrush;
    painter->setBrush(_brush);

    painter->drawRect(geometry_.rect());

    if (option->state & (QStyle::State_Selected | QStyle::State_HasFocus)) {
        auto color = QColor("#969696");

        painter->setPen(QPen(color, 1, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(geometry_.rect());

        painter->setPen(QPen(color, 1, Qt::SolidLine));
        painter->setBrush(Qt::white);

        painter->drawRects(geometry_.anchors());
    }
}

void GraphicsRectItem::resize(const ResizerF& g, ResizerLocation)
{
    prepareGeometryChange();

    geometry_.coords(g.point1(), g.point2());

    update();
}

QGraphicsItem *GraphicsRectItem::copy() const
{
    auto item = new GraphicsRectItem(geometry_.point1(), geometry_.point2());

    item->pen_    = pen_;
    item->brush_  = brush_;
    item->filled_ = filled_;

    return item;
}

///

GraphicsEllipseleItem::GraphicsEllipseleItem(QGraphicsItem *parent)
    : GraphicsItem(parent)
{}

GraphicsEllipseleItem::GraphicsEllipseleItem(const QPointF& x1, const QPointF& x2, QGraphicsItem *parent)
    : GraphicsItem(parent)
{
    setVertexes(x1, x2);
}

void GraphicsEllipseleItem::setVertexes(const QPointF& p1, const QPointF& p2)
{
    if (geometry_.point1() == p1 && geometry_.point2() == p2) return;

    prepareGeometryChange();
    geometry_.coords(p1, p2);
    update();
}

void GraphicsEllipseleItem::push(const QPointF& point)
{
    if (geometry_.point2() == point) return;
    prepareGeometryChange();
    geometry_.point2(point);
    update();
}

QRectF GraphicsEllipseleItem::boundingRect() const { return geometry_.boundingRect(); }

QPainterPath GraphicsEllipseleItem::shape() const
{
    QPainterPath path;
    path.addRect(geometry_.boundingRect());
    return path;
}

ResizerLocation GraphicsEllipseleItem::location(const QPointF& p) const
{
    if (geometry_.isAnchor(p) || geometry_.isRotateAnchor(p)) {
        return geometry_.absolutePos(p);
    }
    else if (filled()) {
        return shape().contains(p) ? ResizerLocation::BORDER : ResizerLocation::OUTSIDE;
    }
    else {
        auto half = std::max(pen_.widthF(), geometry_.borderWidth());

        QRegion r1(geometry_.boundingRect().toRect(), QRegion::Ellipse);
        QRegion r2(geometry_.rect().toRect().adjusted(half, half, -half, -half), QRegion::Ellipse);

        if (r2.contains(p.toPoint()))
            return ResizerLocation::EMPTY_INSIDE;
        else if (r1.contains(p.toPoint()) && !r2.contains(p.toPoint()))
            return ResizerLocation::BORDER;
        else
            return ResizerLocation::OUTSIDE;
    }
}

QPen GraphicsEllipseleItem::pen() const { return pen_; }

void GraphicsEllipseleItem::setPen(const QPen& pen)
{
    if (pen_ == pen) return;

    prepareGeometryChange();
    pen_ = pen;
    update();
}

QBrush GraphicsEllipseleItem::brush() const { return brush_; }

void GraphicsEllipseleItem::setBrush(const QBrush& brush)
{
    if (brush_ == brush) return;

    brush_ = brush;
    update();
}

bool GraphicsEllipseleItem::filled() const { return filled_; }

void GraphicsEllipseleItem::fill(bool f)
{
    if (filled_ == f) return;

    filled_ = f;
    update();
}

void GraphicsEllipseleItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    if (invalid()) return;

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    painter->setPen(pen_);
    auto _brush = filled() ? ((brush_ == Qt::NoBrush) ? pen_.color() : brush_) : Qt::NoBrush;
    painter->setBrush(_brush);

    painter->drawEllipse(geometry_.rect());

    if (option->state & (QStyle::State_Selected | QStyle::State_HasFocus)) {
        auto color = QColor("#969696");

        painter->setPen(QPen(color, 1, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(geometry_.rect());

        painter->setPen(QPen(color, 1, Qt::SolidLine));
        painter->setBrush(Qt::white);

        painter->drawRects(geometry_.anchors());
    }
}

void GraphicsEllipseleItem::resize(const ResizerF& g, ResizerLocation)
{
    prepareGeometryChange();

    geometry_.coords(g.point1(), g.point2());

    update();
}

QGraphicsItem *GraphicsEllipseleItem::copy() const
{
    auto item = new GraphicsEllipseleItem(geometry_.point1(), geometry_.point2());

    item->pen_    = pen_;
    item->brush_  = brush_;
    item->filled_ = filled_;

    // TODO
    return item;
}

///

GraphicsPixmapItem::GraphicsPixmapItem(const QPixmap& pixmap, const QPointF& center, QGraphicsItem *parent)
    : GraphicsItem(parent)
{
    QRectF rect{ { 0, 0 }, pixmap.size() };
    rect.moveCenter(center);

    setPos(rect.topLeft());

    setPixmap(pixmap);

    geometry_.enableRotate(true);
}

void GraphicsPixmapItem::setPixmap(const QPixmap& pixmap)
{
    if (pixmap == pixmap_) return;

    prepareGeometryChange();
    pixmap_ = pixmap;

    geometry_.coords(QRectF{ { 0, 0 }, pixmap.size() });

    update();
}

QRectF GraphicsPixmapItem::boundingRect() const { return shape().controlPointRect(); }

QPainterPath GraphicsPixmapItem::shape() const
{
    QPainterPath path;
    path.addRect(geometry_.boundingRect());
    return path;
}

void GraphicsPixmapItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    painter->drawPixmap(geometry_.rect(), pixmap_, QRectF{ {}, pixmap_.size() });

    if (option->state & (QStyle::State_Selected | QStyle::State_HasFocus)) {
        auto color = QColor("#969696");

        painter->setPen(QPen(color, 1, Qt::DashLine));
        painter->drawRect(geometry_.rect());

        painter->setPen(QPen(color, 1.5, Qt::SolidLine));
        painter->setBrush(Qt::white);

        painter->drawEllipse(geometry_.X1Y1Anchor());
        painter->drawEllipse(geometry_.X1Y2Anchor());
        painter->drawEllipse(geometry_.X2Y1Anchor());
        painter->drawEllipse(geometry_.X2Y2Anchor());

        painter->drawLine(geometry_.rotateAnchor().center(), geometry_.topAnchor().center());
        painter->drawEllipse(geometry_.rotateAnchor());
    }
}

ResizerLocation GraphicsPixmapItem::location(const QPointF& p) const
{
    ResizerLocation l = geometry_.absolutePos(p);
    if (l == ResizerLocation::EMPTY_INSIDE && filled()) return ResizerLocation::BORDER;
    return l;
}

void GraphicsPixmapItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    auto cpos = event->pos().toPoint();

    switch (adjusting_) {
    case canvas::adjusting_t::moving:
    case canvas::adjusting_t::resizing: GraphicsItem::mouseMoveEvent(event); break;

    case canvas::adjusting_t::rotating: {
        auto a = mpos_;
        auto b = mapToScene(geometry_.rect().center());
        auto c = event->scenePos();

        QPointF ab = { b.x() - a.x(), b.y() - a.y() };
        QPointF cb = { b.x() - c.x(), b.y() - c.y() };

        float dot   = (ab.x() * cb.x() + ab.y() * cb.y()); // dot product
        float cross = (ab.x() * cb.y() - ab.y() * cb.x()); // cross product

        onrotate(angle_);
        rotate(angle_ + std::atan2(cross, dot) * 180. / std::numbers::pi);

        mpos_ = event->scenePos();
        break;
    }
    default: break;
    }
}

void GraphicsPixmapItem::resize(const ResizerF& resizer, ResizerLocation location)
{
    auto oldrect = geometry_.rect();
    auto tmprect = resizer.rect();

    auto oldsize = oldrect.size();

    QSizeF newsize = tmprect.contains(tmprect.united(oldrect))
                         ? oldsize.scaled(tmprect.united(oldrect).size(), Qt::KeepAspectRatio)
                         : oldsize.scaled(tmprect.intersected(oldrect).size(), Qt::KeepAspectRatio);

    auto lt = mapToScene(oldrect.topLeft());
    auto lb = mapToScene(oldrect.bottomLeft());
    auto rt = mapToScene(oldrect.topRight());
    auto rb = mapToScene(oldrect.bottomRight());

    prepareGeometryChange();

    // fix me: v2 is on the topleft of v1
    geometry_.coords(resizer.topLeft(), QRectF{ resizer.topLeft(), newsize }.bottomRight());

    // update rotation origin point
    auto center = resizer.rect().center();
    setTransform(
        QTransform().translate(center.x(), center.y()).rotate(angle_).translate(-center.x(), -center.y()));

    // keep position after rotation
    QRectF newrect{ geometry_.rect() };

    // position
    switch (location) {
    case ResizerLocation::X1Y1_ANCHOR: setPos(pos() + rb - mapToScene(newrect.bottomRight())); break;
    case ResizerLocation::X2Y1_ANCHOR: setPos(pos() + lb - mapToScene(newrect.bottomLeft())); break;
    case ResizerLocation::X1Y2_ANCHOR: setPos(pos() + rt - mapToScene(newrect.topRight())); break;
    case ResizerLocation::X2Y2_ANCHOR: setPos(pos() + lt - mapToScene(newrect.topLeft())); break;
    default: break;
    }

    update();
}

void GraphicsPixmapItem::rotate(qreal angle)
{
    angle_ = angle;

    QRectF br{ geometry_.rect() };

    QTransform transform;
    transform.translate(br.center().x(), br.center().y());
    transform.rotate(angle);
    transform.translate(-br.center().x(), -br.center().y());
    setTransform(transform);
}

QGraphicsItem *GraphicsPixmapItem::copy() const
{
    auto item = new GraphicsPixmapItem(pixmap_, boundingRect().center());

    item->rotate(angle_);
    return item;
}

///

int GraphicsCounterleItem::counter = 0;

GraphicsCounterleItem::GraphicsCounterleItem(const QPointF& pos, int v, QGraphicsItem *parent)
    : GraphicsItem(parent)
{
    setPos(pos - QPointF{ 12, 12 });
    setCounter(v);
}

void GraphicsCounterleItem::setCounter(int v)
{
    counter_ = (v == -1) ? counter : v;
    counter  = counter_ + 1;
}

QRectF GraphicsCounterleItem::boundingRect() const { return QRectF{ 0, 0, 25, 25 }; }

QPainterPath GraphicsCounterleItem::shape() const
{
    QPainterPath path;
    path.addRect(boundingRect()); // to include corner anchors
    return shape_from_path(path, pen_);
}

ResizerLocation GraphicsCounterleItem::location(const QPointF&) const { return ResizerLocation::BORDER; }

void GraphicsCounterleItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    painter->setPen(QPen{ Qt::red });
    painter->setBrush(Qt::red);

    painter->drawEllipse({ 13, 13 }, 12, 12);

    painter->setPen(QPen{ Qt::white });

    painter->drawText(QRectF{ 1, 1, 23, 23 }, Qt::AlignCenter, QString::number(counter_));

    if (option->state & (QStyle::State_Selected | QStyle::State_HasFocus)) {
        auto color = QColor("#969696");

        painter->setPen(QPen(color, 1, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse({ 13, 13 }, 12, 12);
    }
}

///

GraphicsPathItem::GraphicsPathItem(const QPointF& p, QGraphicsItem *parent)
    : GraphicsItem(parent)
{
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setAcceptHoverEvents(false);
    setAcceptedMouseButtons(Qt::NoButton);
    setCacheMode(QGraphicsItem::DeviceCoordinateCache);

    path_.moveTo(p);
}

QRectF GraphicsPathItem::boundingRect() const { return shape().controlPointRect(); }

QPainterPath GraphicsPathItem::shape() const { return shape_from_path(path_, pen_); }

bool GraphicsPathItem::contains(const QPointF& point) const { return QGraphicsItem::contains(point); }

QPen GraphicsPathItem::pen() const { return pen_; }

void GraphicsPathItem::setPen(const QPen& pen)
{
    if (pen_ == pen) return;

    // onchange(std::make_shared<PenChangedCommand>(this, pen_, pen));

    prepareGeometryChange();
    pen_ = pen;
    update();
}

void GraphicsPathItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    pen_.setCapStyle(Qt::RoundCap);
    pen_.setJoinStyle(Qt::RoundJoin);

    painter->setRenderHint(QPainter::Antialiasing);

    painter->setPen(pen_);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path_);
}

void GraphicsPathItem::push(const QPointF& p)
{
    prepareGeometryChange();
    path_.lineTo(p);
    update();
}

void GraphicsPathItem::pushVertexes(const QVector<QPointF>& points)
{
    prepareGeometryChange();
    for (int i = 0; i < points.size(); ++i) {
        path_.lineTo(points[i]);
    }
    update();
}

///

GraphicsCurveItem::GraphicsCurveItem(const QPointF& p, QGraphicsItem *parent)
    : GraphicsPathItem(p, parent)
{}

GraphicsMosaicItem::GraphicsMosaicItem(const QPointF& p, QGraphicsItem *parent)
    : GraphicsPathItem(p, parent)
{
    setFlag(QGraphicsItem::ItemIsFocusable, false);
}

GraphicsEraserItem::GraphicsEraserItem(const QPointF& p, QGraphicsItem *parent)
    : GraphicsPathItem(p, parent)
{
    setFlag(QGraphicsItem::ItemIsFocusable, false);
}

///

GraphicsTextItem::GraphicsTextItem(const QPointF& pos, QGraphicsItem *parent)
    : GraphicsTextItem("", pos, parent)
{}

GraphicsTextItem::GraphicsTextItem(const QString& text, const QPointF& pos, QGraphicsItem *parent)
    : GraphicsItemInterface(),
      QGraphicsTextItem(text, parent)
{
    setPos(pos);

    setFlag(QGraphicsItem::ItemIsFocusable);
    setFlag(QGraphicsItem::ItemIsMovable);
    setFlag(QGraphicsItem::ItemIsSelectable);
    setTextInteractionFlags(Qt::TextEditorInteraction);
    setAcceptHoverEvents(true);
}

QRectF GraphicsTextItem::paddingRect() const
{
    return QGraphicsTextItem::boundingRect().adjusted(-padding, -padding, padding, padding);
}

QRectF GraphicsTextItem::boundingRect() const
{
    const auto hw = 12.0 / 2.0;
    ResizerF resizer{ paddingRect() };
    return paddingRect().adjusted(-hw, -hw, hw, hw).united(resizer.rotateAnchor());
}

QPainterPath GraphicsTextItem::shape() const
{
    QPainterPath path;
    path.addRect(boundingRect());
    return path;
}

ResizerLocation GraphicsTextItem::location(const QPointF& p) const
{
    ResizerF resizer{ paddingRect(), static_cast<float>(12) };
    resizer.enableRotate(true);

    ResizerLocation loc = resizer.absolutePos(p, true, false);

    if (loc == ResizerLocation::FILLED_INSIDE && textInteractionFlags() != Qt::NoTextInteraction) {
        loc = ResizerLocation::EDITING_INSIDE;
    }

    return loc;
}

void GraphicsTextItem::setFont(const QFont& f)
{
    if (adjusting_ != canvas::adjusting_t::resizing && f != font()) {
        QGraphicsTextItem::setFont(f);
    }
}

void GraphicsTextItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    // render text
    QStyleOptionGraphicsItem text_render_option = *option;
    text_render_option.state = option->state & ~(QStyle::State_Selected | QStyle::State_HasFocus);

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::TextAntialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    QGraphicsTextItem::paint(painter, &text_render_option, nullptr);

    // render resizer
    if (option->state & (QStyle::State_Selected | QStyle::State_HasFocus)) {
        auto color = QColor("#969696");

        painter->setPen(QPen(color, 1, Qt::DashLine));
        painter->drawRect(paddingRect());

        painter->setPen(QPen(color, 1.5, Qt::SolidLine));
        painter->setBrush(Qt::white);

        ResizerF resizer{ paddingRect() };
        painter->drawEllipse(resizer.X1Y1Anchor());
        painter->drawEllipse(resizer.X1Y2Anchor());
        painter->drawEllipse(resizer.X2Y1Anchor());
        painter->drawEllipse(resizer.X2Y2Anchor());

        painter->drawLine(resizer.rotateAnchor().center(), resizer.topAnchor().center());
        painter->drawEllipse(resizer.rotateAnchor());
    }
}

void GraphicsTextItem::focusInEvent(QFocusEvent *event)
{
    if (event->reason() == Qt::MouseFocusReason) onfocus();

    QGraphicsTextItem::focusInEvent(event);
}

void GraphicsTextItem::focusOutEvent(QFocusEvent *event)
{
    adjusting_ = canvas::adjusting_t::none;
    onblur();
    setTextInteractionFlags(Qt::NoTextInteraction);

    QGraphicsTextItem::focusOutEvent(event);
}

void GraphicsTextItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    auto hpos = location(event->pos());

    if (hover_location_ != hpos) {
        onhover(hpos);
        hover_location_ = hpos;
    }

    QGraphicsTextItem::hoverMoveEvent(event);
}

void GraphicsTextItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    onhover(ResizerLocation::DEFAULT);
    hover_location_ = ResizerLocation::DEFAULT;
    QGraphicsTextItem::hoverLeaveEvent(event);
}

void GraphicsTextItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (!!(hover_location_ & ResizerLocation::ANCHOR) && !toPlainText().isEmpty()) {
            adjusting_       = canvas::adjusting_t::resizing;
            before_resizing_ = ResizerF{ QGraphicsTextItem::boundingRect() };
        }
        else if (!!(hover_location_ & ResizerLocation::BORDER) ||
                 hover_location_ == ResizerLocation::FILLED_INSIDE) {
            adjusting_     = canvas::adjusting_t::moving;
            mpos_          = event->scenePos();
            before_moving_ = pos();
        }
        else if (hover_location_ == ResizerLocation::ROTATE_ANCHOR) {
            adjusting_       = canvas::adjusting_t::rotating;
            mpos_            = event->scenePos();
            before_rotating_ = angle_;
        }
    }
    else {
        setTextInteractionFlags(Qt::NoTextInteraction);
    }

    setFocus(Qt::MouseFocusReason);
    if (!hasFocus() && !isSelected()) onfocus();

    QGraphicsTextItem::mousePressEvent(event);
}

void GraphicsTextItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    auto cpos = event->pos();

    switch (adjusting_) {
    case canvas::adjusting_t::moving: {
        auto offset = event->scenePos() - mpos_;
        moveBy(offset.x(), offset.y());
        mpos_ = event->scenePos();
        break;
    }
    case canvas::adjusting_t::rotating: {
        auto a = mpos_;
        auto b = mapToScene(QGraphicsTextItem::boundingRect().center());
        auto c = event->scenePos();

        QPointF ab = { b.x() - a.x(), b.y() - a.y() };
        QPointF cb = { b.x() - c.x(), b.y() - c.y() };

        float dot   = (ab.x() * cb.x() + ab.y() * cb.y()); // dot product
        float cross = (ab.x() * cb.y() - ab.y() * cb.x()); // cross product

        rotate(angle_ + std::atan2(cross, dot) * 180. / std::numbers::pi);

        mpos_ = event->scenePos();

        break;
    }
    case canvas::adjusting_t::resizing: {
        auto resizer = ResizerF(QGraphicsTextItem::boundingRect());

        switch (hover_location_) {
        case ResizerLocation::X1Y1_ANCHOR: resizer.x1(cpos.x()), resizer.y1(cpos.y()); break;
        case ResizerLocation::X2Y1_ANCHOR: resizer.x2(cpos.x()), resizer.y1(cpos.y()); break;
        case ResizerLocation::X1Y2_ANCHOR: resizer.x1(cpos.x()), resizer.y2(cpos.y()); break;
        case ResizerLocation::X2Y2_ANCHOR: resizer.x2(cpos.x()), resizer.y2(cpos.y()); break;
        default: break;
        }

        resize(resizer, hover_location_);

        break;
    }
    default: QGraphicsTextItem::mouseMoveEvent(event); break;
    }
}

void GraphicsTextItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    switch (adjusting_) {
    case canvas::adjusting_t::moving: onmove(before_moving_); break;
    case canvas::adjusting_t::resizing: onresize(before_resizing_, hover_location_); break;
    case canvas::adjusting_t::rotating: onrotate(before_rotating_); break;
    default: break;
    }

    adjusting_       = canvas::adjusting_t::none;
    mpos_            = {};
    before_rotating_ = {};
    before_resizing_ = {};
    before_moving_   = {};
    QGraphicsTextItem::mouseReleaseEvent(event);
}

void GraphicsTextItem::resize(const ResizerF& resizer, ResizerLocation location)
{
    auto textrect_1   = QGraphicsTextItem::boundingRect();
    auto resized_rect = resizer.rect();
    auto textsize_1   = textrect_1.size();

    QSizeF textsize_2 =
        resized_rect.contains(resized_rect.united(textrect_1))
            ? textsize_1.scaled(resized_rect.united(textrect_1).size(), Qt::KeepAspectRatio)
            : textsize_1.scaled(resized_rect.intersected(textrect_1).size(), Qt::KeepAspectRatio);

    auto scaled_fontsize = std::max(font().pointSizeF() * (textsize_2.width() / textsize_1.width()), 5.0);

    if (scaled_fontsize == font().pointSizeF()) return;

    auto lt = mapToScene(textrect_1.topLeft());
    auto lb = mapToScene(textrect_1.bottomLeft());
    auto rt = mapToScene(textrect_1.topRight());
    auto rb = mapToScene(textrect_1.bottomRight());

    prepareGeometryChange();

    // resized font size
    auto scaledfont = font();
    scaledfont.setPointSizeF(scaled_fontsize);
    QGraphicsTextItem::setFont(scaledfont);

    // update rotation origin point
    auto center = QGraphicsTextItem::boundingRect().center();
    setTransform(
        QTransform().translate(center.x(), center.y()).rotate(angle_).translate(-center.x(), -center.y()));

    // keep position after rotation
    auto textrect_2 = QGraphicsTextItem::boundingRect();

    // position
    switch (location) {
    case ResizerLocation::X1Y1_ANCHOR: setPos(pos() + rb - mapToScene(textrect_2.bottomRight())); break;
    case ResizerLocation::X2Y1_ANCHOR: setPos(pos() + lb - mapToScene(textrect_2.bottomLeft())); break;
    case ResizerLocation::X1Y2_ANCHOR: setPos(pos() + rt - mapToScene(textrect_2.topRight())); break;
    case ResizerLocation::X2Y2_ANCHOR: setPos(pos() + lt - mapToScene(textrect_2.topLeft())); break;
    default: break;
    }

    update();
}

void GraphicsTextItem::rotate(qreal angle)
{
    angle_ = angle;

    QTransform transform;
    transform.translate(QGraphicsTextItem::boundingRect().center().x(),
                        QGraphicsTextItem::boundingRect().center().y());
    transform.rotate(angle);
    transform.translate(-QGraphicsTextItem::boundingRect().center().x(),
                        -QGraphicsTextItem::boundingRect().center().y());
    setTransform(transform);
}

void GraphicsTextItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    if (textInteractionFlags() != Qt::TextEditorInteraction) {
        setTextInteractionFlags(Qt::TextEditorInteraction);
        setFocus(Qt::MouseFocusReason);
        adjusting_ = canvas::adjusting_t::editing;

        auto cursor = textCursor();
        cursor.setPosition(document()->documentLayout()->hitTest(event->pos(), Qt::FuzzyHit));
        setTextCursor(cursor);
    }
    else {
        QGraphicsTextItem::mouseDoubleClickEvent(event);
    }
}

QGraphicsItem *GraphicsTextItem::copy() const
{
    auto item = new GraphicsTextItem(pos());
    item->setPlainText(toPlainText());

    item->rotate(angle_);

    item->setPen(pen());
    item->setFont(font());
    return item;
}