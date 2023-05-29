#include "graphicsitems.h"

#include "logging.h"

#include <numbers>
#include <QAbstractTextDocumentLayout>
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
    setFlag(QGraphicsItem::ItemIsSelectable);
    setAcceptHoverEvents(true);
}

void GraphicsItem::focusInEvent(QFocusEvent *event)
{
    if (event->reason() == Qt::MouseFocusReason) onfocus();

    QGraphicsItem::focusInEvent(event);
}

void GraphicsItem::focusOutEvent(QFocusEvent *event)
{
    onblur();
    QGraphicsItem::focusOutEvent(event);
}

void GraphicsItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    auto hpos = location(event->pos());

    if (hpos != hover_location_) {
        onhover(hpos);
        hover_location_ = hpos;

        setCursor(getCursorByLocation(hpos));
    }

    QGraphicsItem::hoverEnterEvent(event);
}

void GraphicsItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    auto hpos = location(event->pos());

    if (hpos != hover_location_) {
        onhover(hpos);
        hover_location_ = hpos;

        setCursor(getCursorByLocation(hpos));
    }

    QGraphicsItem::hoverMoveEvent(event);
}

void GraphicsItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    onhover(Resizer::DEFAULT);
    hover_location_ = Resizer::DEFAULT;
    unsetCursor();

    QGraphicsItem::hoverLeaveEvent(event);
}

void GraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (hover_location_ & Resizer::ANCHOR) {
            adjusting_ = adjusting_t::resizing;
        }
        else if (hover_location_ & Resizer::BORDER) {
            adjusting_ = adjusting_t::moving;
        }
    }

    QGraphicsItem::mousePressEvent(event);
}

void GraphicsItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) { QGraphicsItem::mouseMoveEvent(event); }

void GraphicsItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    adjusting_ = adjusting_t::none;
    QGraphicsItem::mouseReleaseEvent(event);
}

void GraphicsItem::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete) {
        setVisible(false);
        onchange(std::make_shared<DeletedCommand>(this));
    }
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
    if (v1_ == p1 && v2_ == p2) return;
    prepareGeometryChange();
    v1_ = p1;
    v2_ = p2;
    update();
}

void GraphicsLineItem::push(const QPointF& point)
{
    if (v2_ == point) return;
    prepareGeometryChange();
    v2_ = point;
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

QRectF GraphicsLineItem::boundingRect() const
{
    if (pen_.widthF() == 0.0) {
        auto lx = std::min(v1_.x(), v2_.x());
        auto rx = std::max(v1_.x(), v2_.x());
        auto ty = std::min(v1_.y(), v2_.y());
        auto by = std::max(v1_.y(), v2_.y());
        return QRectF(lx, ty, rx - lx, by - ty);
    }
    return shape().controlPointRect();
}

QPainterPath GraphicsLineItem::shape() const
{
    QPainterPath path;
    if (v1_ == QPointF{} && v2_ == QPointF{}) return path;

    path.moveTo(v1_);
    path.lineTo(v2_);

    QPen _pen(pen_);
    _pen.setWidth(std::max(pen_.widthF(), adjusting_min_w_));
    return shape_from_path(path, _pen);
}

Resizer::PointPosition GraphicsLineItem::location(const QPointF& p) const
{
    ResizerF resizer{ v1_, v2_, std::max<float>(pen_.widthF(), adjusting_min_w_) };
    if (resizer.isX1Y1Anchor(p)) {
        return Resizer::X1Y1_ANCHOR;
    }
    else if (resizer.isX2Y2Anchor(p)) {
        return Resizer::X2Y2_ANCHOR;
    }
    else if (shape().contains(p)) {
        return Resizer::BORDER;
    }

    return Resizer::OUTSIDE;
}

bool GraphicsLineItem::contains(const QPointF& point) const { return QGraphicsItem::contains(point); }

void GraphicsLineItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    if (invalid()) return;

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    painter->setPen(pen_);
    painter->drawLine(v1_, v2_);

    if (option->state & QStyle::State_HasFocus) {
        auto color = QColor("#969696");

        painter->setPen(QPen(color, 1, Qt::DashLine));
        painter->drawLine(v1_, v2_);

        painter->setPen(QPen(color, 1, Qt::SolidLine));
        painter->setBrush(Qt::white);
        ResizerF resizer(v1_, v2_);
        painter->drawRect(resizer.X1Y1Anchor());
        painter->drawRect(resizer.X2Y2Anchor());
    }
}

void GraphicsLineItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    GraphicsItem::hoverMoveEvent(event);
}

void GraphicsLineItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    GraphicsItem::mousePressEvent(event);
}

void GraphicsLineItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    auto cursor_pos = event->pos();

    switch (adjusting_) {
    case adjusting_t::moving: QGraphicsItem::mouseMoveEvent(event); break;
    case adjusting_t::resizing: {
        prepareGeometryChange();

        ResizerF resizer(v1_, v2_);
        // clang-format off
        switch (hover_location_) {
        case Resizer::X1Y1_ANCHOR:  resizer.x1(cursor_pos.x()); resizer.y1(cursor_pos.y()); break;
        case Resizer::X2Y2_ANCHOR:  resizer.x2(cursor_pos.x()); resizer.y2(cursor_pos.y()); break;
        default: break;
        }
        // clang-format on
        v1_ = resizer.point1();
        v2_ = resizer.point2();

        update();

        break;
    }
    default: break;
    }
}

void GraphicsLineItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    GraphicsItem::mouseReleaseEvent(event);
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
    if (polygon_[0] == p1 && polygon_[3] == p2) return;
    prepareGeometryChange();
    polygon_ = compute_arrow_vertexes(p1, p2);
    update();
}

void GraphicsArrowItem::push(const QPointF& point)
{
    if (polygon_[3] == point) return;
    prepareGeometryChange();
    polygon_ = compute_arrow_vertexes(polygon_[0], point);
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

QRectF GraphicsArrowItem::boundingRect() const { return shape().controlPointRect().adjusted(-4, -4, 4, 4); }

QPainterPath GraphicsArrowItem::shape() const
{
    QPainterPath path;
    path.addPolygon(polygon_);
    return shape_from_path(path, { pen_.color(), 3, Qt::SolidLine });
}

Resizer::PointPosition GraphicsArrowItem::location(const QPointF& p) const
{
    ResizerF resizer{ polygon_[0], polygon_[3], std::max<float>(pen_.widthF(), adjusting_min_w_) };
    if (resizer.isX1Y1Anchor(p)) {
        return Resizer::X1Y1_ANCHOR;
    }
    else if (resizer.isX2Y2Anchor(p)) {
        return Resizer::X2Y2_ANCHOR;
    }
    else if (shape().contains(p)) {
        return Resizer::BORDER;
    }
}

bool GraphicsArrowItem::contains(const QPointF& point) const { return QGraphicsItem::contains(point); }

void GraphicsArrowItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    if (invalid()) return;

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    painter->setPen({ pen_.color(), 1, Qt::SolidLine });
    painter->setBrush(pen_.color());
    painter->drawPolygon(polygon_);

    if (option->state & QStyle::State_HasFocus) {
        auto color = QColor("#969696");

        painter->setPen(QPen(color, 1, Qt::DashLine));
        painter->drawLine(polygon_[0], polygon_[3]);

        painter->setPen(QPen(color, 1, Qt::SolidLine));
        painter->setBrush(Qt::white);
        ResizerF resizer{ polygon_[0], polygon_[3] };
        painter->drawRect(resizer.X1Y1Anchor());
        painter->drawRect(resizer.X2Y2Anchor());
    }
}

void GraphicsArrowItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    GraphicsItem::hoverMoveEvent(event);
}

void GraphicsArrowItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    GraphicsItem::mousePressEvent(event);
}

void GraphicsArrowItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    auto cursor_pos = event->pos();

    switch (adjusting_) {
    case adjusting_t::moving: QGraphicsItem::mouseMoveEvent(event); break;
    case adjusting_t::resizing: {
        prepareGeometryChange();

        ResizerF resizer(polygon_[0], polygon_[3]);
        // clang-format off
        switch (hover_location_) {
        case Resizer::X1Y1_ANCHOR:  resizer.x1(cursor_pos.x()); resizer.y1(cursor_pos.y()); break;
        case Resizer::X2Y2_ANCHOR:  resizer.x2(cursor_pos.x()); resizer.y2(cursor_pos.y()); break;
        default: break;
        }
        // clang-format on
        polygon_ = compute_arrow_vertexes(resizer.point1(), resizer.point2());

        update();

        break;
    }
    default: break;
    }
}

void GraphicsArrowItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    GraphicsItem::mouseReleaseEvent(event);
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
    if (v1_ == p1 && v2_ == p2) return;
    prepareGeometryChange();
    v1_ = p1;
    v2_ = p2;
    update();
}

void GraphicsRectItem::push(const QPointF& point)
{
    if (v2_ == point) return;
    prepareGeometryChange();
    v2_ = point;
    update();
}

QRectF GraphicsRectItem::boundingRect() const { return shape().boundingRect(); }

QPainterPath GraphicsRectItem::shape() const
{
    QPainterPath path;
    path.addRect(QRectF{ v1_, v2_ });

    QPen _pen(pen_);
    _pen.setWidth(std::max(pen_.widthF(), adjusting_min_w_));
    return shape_from_path(path, _pen);
}

Resizer::PointPosition GraphicsRectItem::location(const QPointF& p) const
{
    ResizerF resizer{ v1_, v2_, std::max<float>(pen_.widthF(), adjusting_min_w_) };

    ResizerF::PointPosition l = resizer.absolutePos(p);
    if (l == ResizerF::INSIDE && filled()) return Resizer::BORDER;
    return static_cast<Resizer::PointPosition>(l);
}

bool GraphicsRectItem::contains(const QPointF& point) const { return QGraphicsItem::contains(point); }

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

    painter->setPen(pen_);
    auto _brush = filled() ? ((brush_ == Qt::NoBrush) ? pen_.color() : brush_) : Qt::NoBrush;
    painter->setBrush(_brush);

    painter->drawRect({ v1_, v2_ });

    if (option->state & QStyle::State_HasFocus) {
        auto color = QColor("#969696");

        painter->setPen(QPen(color, 1, Qt::DashLine));
        painter->drawRect({ v1_, v2_ });

        painter->setPen(QPen(color, 1, Qt::SolidLine));
        painter->setBrush(Qt::white);

        ResizerF resizer{ v1_, v2_ };
        painter->drawRects(resizer.anchors());
    }
}

void GraphicsRectItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    GraphicsItem::hoverMoveEvent(event);
}

void GraphicsRectItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    GraphicsItem::mousePressEvent(event);
}

void GraphicsRectItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    auto cursor_pos = event->pos().toPoint();

    switch (adjusting_) {
    case adjusting_t::moving: QGraphicsItem::mouseMoveEvent(event); break;
    case adjusting_t::resizing: {
        prepareGeometryChange();

        ResizerF resizer(v1_, v2_);
        // clang-format off
        switch (hover_location_) {
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
        v1_ = resizer.point1();
        v2_ = resizer.point2();

        update();

        break;
    }
    default: break;
    }
}

void GraphicsRectItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    GraphicsItem::mouseReleaseEvent(event);
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
    if (v1_ == p1 && v2_ == p2) return;
    prepareGeometryChange();
    v1_ = p1;
    v2_ = p2;
    update();
}

void GraphicsEllipseleItem::push(const QPointF& point)
{
    if (v2_ == point) return;
    prepareGeometryChange();
    v2_ = point;
    update();
}

QRectF GraphicsEllipseleItem::boundingRect() const
{
    auto hw = std::max(pen_.widthF(), adjusting_min_w_) / 2;
    return ResizerF{ v1_, v2_ }.rect().adjusted(-hw, -hw, hw, hw);
}

QPainterPath GraphicsEllipseleItem::shape() const
{
    QPainterPath path;
    path.addRect(QRectF{ v1_, v2_ }); // to include corner anchors

    QPen _pen(pen_);
    _pen.setWidth(std::max(pen_.widthF(), adjusting_min_w_));
    return shape_from_path(path, _pen);
}

Resizer::PointPosition GraphicsEllipseleItem::location(const QPointF& p) const
{
    ResizerF resizer{ v1_, v2_, std::max<float>(pen_.widthF(), adjusting_min_w_) };

    if (resizer.isAnchor(p) || resizer.isRotateAnchor(p)) {
        return static_cast<Resizer::PointPosition>(resizer.absolutePos(p));
    }
    else if (filled()) {
        return shape().contains(p) ? Resizer::BORDER : Resizer::OUTSIDE;
    }
    else {
        auto half = std::max(pen_.widthF(), adjusting_min_w_);

        QRegion r1(ResizerF{ v1_, v2_ }.rect().toRect().adjusted(-half, -half, half, half),
                   QRegion::Ellipse);
        QRegion r2(ResizerF{ v1_, v2_ }.rect().toRect().adjusted(half, half, -half, -half),
                   QRegion::Ellipse);

        if (r2.contains(p.toPoint()))
            return Resizer::INSIDE;
        else if (r1.contains(p.toPoint()) && !r2.contains(p.toPoint()))
            return Resizer::BORDER;
        else
            return Resizer::OUTSIDE;
    }
}

bool GraphicsEllipseleItem::contains(const QPointF& point) const { return QGraphicsItem::contains(point); }

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

    painter->drawEllipse({ v1_, v2_ });

    if (option->state & QStyle::State_HasFocus) {
        auto color = QColor("#969696");

        painter->setPen(QPen(color, 1, Qt::DashLine));
        painter->drawRect({ v1_, v2_ });

        painter->setPen(QPen(color, 1, Qt::SolidLine));
        painter->setBrush(Qt::white);

        ResizerF resizer{ v1_, v2_ };
        painter->drawRects(resizer.anchors());
    }
}

void GraphicsEllipseleItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    GraphicsItem::hoverMoveEvent(event);
}

void GraphicsEllipseleItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    GraphicsItem::mousePressEvent(event);
}

void GraphicsEllipseleItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    auto cursor_pos = event->pos().toPoint();

    switch (adjusting_) {
    case adjusting_t::moving: QGraphicsItem::mouseMoveEvent(event); break;
    case adjusting_t::resizing: {
        prepareGeometryChange();

        ResizerF resizer(v1_, v2_);
        // clang-format off
        switch (hover_location_) {
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
        v1_ = resizer.point1();
        v2_ = resizer.point2();

        update();

        break;
    }
    default: break;
    }
}

void GraphicsEllipseleItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    GraphicsItem::mouseReleaseEvent(event);
}

///

GraphicsPathItem::GraphicsPathItem(const QPointF& p, QGraphicsItem *parent)
    : GraphicsItem(parent)
{
    path_.moveTo(p);
}

QRectF GraphicsPathItem::boundingRect() const { return shape().controlPointRect(); }

QPainterPath GraphicsPathItem::shape() const { return shape_from_path(path_, pen_); }

bool GraphicsPathItem::contains(const QPointF& point) const { return QGraphicsItem::contains(point); }

QPen GraphicsPathItem::pen() const { return pen_; }

void GraphicsPathItem::setPen(const QPen& pen)
{
    if (pen_ == pen) return;
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
{}

GraphicsEraserItem::GraphicsEraserItem(const QPointF& p, QGraphicsItem *parent)
    : GraphicsPathItem(p, parent)
{}

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
    const auto anchor_hw = 7.0 / 2.0;
    return paddingRect().adjusted(-anchor_hw, -anchor_hw, anchor_hw, anchor_hw);
}

QPainterPath GraphicsTextItem::shape() const 
{
    QPainterPath path;
    path.addRect(boundingRect());
    return path;
}

Resizer::PointPosition GraphicsTextItem::location(const QPointF& p) const
{
    ResizerF resizer{ paddingRect(), static_cast<float>(adjusting_min_w_) };

    ResizerF::PointPosition l = resizer.absolutePos(p);

    if (l == ResizerF::INSIDE && filled()) {
        return hasFocus() ? Resizer::EDITAREA : Resizer::BORDER;
    }

    return static_cast<Resizer::PointPosition>(l);
}

void GraphicsTextItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    auto txt_option  = *option;
    txt_option.state = option->state & ~(QStyle::State_Selected | QStyle::State_HasFocus);

    QGraphicsTextItem::paint(painter, &txt_option, nullptr);

    if (option->state & (QStyle::State_Selected | QStyle::State_HasFocus)) {
        auto color = QColor("#969696");

        painter->setPen(QPen(color, 1, Qt::DashLine));
        painter->drawRect(paddingRect());

        painter->setPen(QPen(color, 1, Qt::SolidLine));
        painter->setBrush(Qt::white);

        ResizerF resizer{ paddingRect() };
        painter->drawRects(resizer.anchors());
    }
}

void GraphicsTextItem::focusInEvent(QFocusEvent *event)
{
    if (event->reason() == Qt::MouseFocusReason) onfocus();

    QGraphicsTextItem::focusInEvent(event);
}

void GraphicsTextItem::focusOutEvent(QFocusEvent *event)
{
    onblur();
    setTextInteractionFlags(Qt::NoTextInteraction);

    QGraphicsTextItem::focusOutEvent(event);
}

void GraphicsTextItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    onhover(Resizer::EDITAREA);

    setCursor(hasFocus() ? Qt::IBeamCursor : Qt::SizeAllCursor);
    QGraphicsTextItem::hoverEnterEvent(event);
}

void GraphicsTextItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    auto hpos = location(event->pos());

    if (hover_location_ != hpos) {
        if (hpos & Resizer::BORDER || hpos & Resizer::ANCHOR) {
            onhover(hover_location_);
            hover_location_ = hpos;
            setCursor(getCursorByLocation(hover_location_));
        }
        else if (hpos == Resizer::INSIDE || hpos == Resizer::EDITAREA) {
            onhover(Resizer::EDITAREA);
            hover_location_ = hpos;
            setCursor(hasFocus() ? Qt::IBeamCursor : Qt::SizeAllCursor);
        }
    }

    QGraphicsTextItem::hoverMoveEvent(event);
}

void GraphicsTextItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    onhover(Resizer::DEFAULT);
    hover_location_ = Resizer::DEFAULT;
    unsetCursor();
    QGraphicsTextItem::hoverLeaveEvent(event);
}

void GraphicsTextItem::keyPressEvent(QKeyEvent *event) { QGraphicsTextItem::keyPressEvent(event); }

void GraphicsTextItem::keyReleaseEvent(QKeyEvent *event) { QGraphicsTextItem::keyReleaseEvent(event); }

void GraphicsTextItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsTextItem::mousePressEvent(event);
}

void GraphicsTextItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsTextItem::mouseMoveEvent(event);
}

void GraphicsTextItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsTextItem::mouseReleaseEvent(event);
}

void GraphicsTextItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (textInteractionFlags() != Qt::TextEditorInteraction) {
        setTextInteractionFlags(Qt::TextEditorInteraction);
        setFocus();
    }

    auto cursor = textCursor();
    cursor.setPosition(document()->documentLayout()->hitTest(event->pos(), Qt::FuzzyHit));
    setTextCursor(cursor);
}
