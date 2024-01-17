#include "graphicsitems.h"

#include "logging.h"

#include <cmath>
#include <numbers>
#include <QAbstractTextDocumentLayout>
#include <QGraphicsScene>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

ResizerLocation GraphicsItemWrapper::location(const QPointF& p) const
{
    return geometry_.absolutePos(p, filled());
}

void GraphicsItemWrapper::hover(QGraphicsSceneHoverEvent *event)
{
    auto hpos = location(event->pos());

    if (hpos != hover_location_) {
        onhover(hpos);
        hover_location_ = hpos;
    }
}

void GraphicsItemWrapper::hoverLeave(QGraphicsSceneHoverEvent *)
{
    onhover(ResizerLocation::DEFAULT);
    hover_location_ = ResizerLocation::DEFAULT;
}

void GraphicsItemWrapper::mousePress(QGraphicsSceneMouseEvent *event, const QPointF& pos)
{
    if (event->button() == Qt::LeftButton) {
        if (!!(hover_location_ & ResizerLocation::ANCHOR)) {
            adjusting_       = canvas::adjusting_t::resizing;
            before_resizing_ = geometry();
        }
        else if (!!(hover_location_ & ResizerLocation::BORDER) ||
                 !!(hover_location_ & ResizerLocation::FILLED_INSIDE)) {
            adjusting_     = canvas::adjusting_t::moving;
            mpos_          = event->scenePos();
            before_moving_ = pos;
        }
        else if (!!(hover_location_ & ResizerLocation::ROTATE_ANCHOR)) {
            adjusting_       = canvas::adjusting_t::rotating;
            mpos_            = event->scenePos();
            before_rotating_ = angle_;
        }
    }
}

void GraphicsItemWrapper::mouseMove(QGraphicsSceneMouseEvent *event, const QPointF& center)
{
    switch (adjusting_) {
    case canvas::adjusting_t::moving: break;
    case canvas::adjusting_t::resizing: {
        auto cpos = event->pos();

        ResizerF resizer = geometry();
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
    case canvas::adjusting_t::rotating: {
        auto a = mpos_;
        auto b = center;
        auto c = event->scenePos();

        QPointF ab = { b.x() - a.x(), b.y() - a.y() };
        QPointF cb = { b.x() - c.x(), b.y() - c.y() };

        float dot   = (ab.x() * cb.x() + ab.y() * cb.y()); // dot product
        float cross = (ab.x() * cb.y() - ab.y() * cb.x()); // cross product

        rotate(angle_ + std::atan2(cross, dot) * 180. / std::numbers::pi);

        mpos_ = event->scenePos();
        break;
    }
    default: break;
    }
}

void GraphicsItemWrapper::mouseRelease(QGraphicsSceneMouseEvent *, const QPointF& pos)
{
    switch (adjusting_) {
    case canvas::adjusting_t::moving:
        if (pos != before_moving_) onmove(before_moving_);
        break;
    case canvas::adjusting_t::resizing: onresize(before_resizing_, hover_location_); break;
    case canvas::adjusting_t::rotating: onrotate(before_rotating_); break;

    default: break;
    }

    adjusting_       = canvas::adjusting_t::none;
    before_resizing_ = {};
    before_moving_   = {};
    mpos_            = {};
    before_rotating_ = {};
}

void GraphicsItemWrapper::rotate(qreal angle)
{
    angle_ = angle;

    auto cx = geometry().center().x(), cy = geometry().center().y();
    dynamic_cast<QGraphicsItem *>(this)->setTransform(
        QTransform{}.translate(cx, cy).rotate(angle).translate(-cx, -cy));
}

////

GraphicsItem::GraphicsItem(QGraphicsItem *parent)
    : QGraphicsItem(parent)
{
    setFlag(QGraphicsItem::ItemIsFocusable);
    setFlag(QGraphicsItem::ItemIsMovable);
    // setFlag(QGraphicsItem::ItemIsSelectable);
    setAcceptHoverEvents(true);
}

QPen GraphicsItem::pen() const { return pen_; }

void GraphicsItem::setPen(const QPen& pen)
{
    if (pen_ == pen) return;

    prepareGeometryChange();
    pen_ = pen;
    update();
}

QRectF GraphicsItem::boundingRect() const { return shape().controlPointRect(); }

void GraphicsItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    GraphicsItemWrapper::hover(event);
    QGraphicsItem::hoverMoveEvent(event);
}

void GraphicsItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    GraphicsItemWrapper::hoverLeave(event);
    QGraphicsItem::hoverLeaveEvent(event);
}

void GraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    GraphicsItemWrapper::mousePress(event, pos());
    if (hover_location_ != ResizerLocation::EMPTY_INSIDE) QGraphicsItem::mousePressEvent(event);
}

void GraphicsItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    (adjusting_ == canvas::adjusting_t::moving)
        ? QGraphicsItem::mouseMoveEvent(event)
        : GraphicsItemWrapper::mouseMove(event, mapToScene(geometry().rect().center()));
}

void GraphicsItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    GraphicsItemWrapper::mouseRelease(event, pos());
    if (hover_location_ != ResizerLocation::EMPTY_INSIDE) QGraphicsItem::mouseReleaseEvent(event);
}

void GraphicsItem::resize(const ResizerF& ng, ResizerLocation)
{
    prepareGeometryChange();

    geometry_.coords(ng.point1(), ng.point2());

    update();
}

///

static QPainterPath shape_from_path(const QPainterPath& path, const QPen& pen)
{
    // We unfortunately need this hack as QPainterPathStroker will set a width of 1.0
    // if we pass a value of 0.0 to QPainterPathStroker::setWidth()
    constexpr auto ZERO = static_cast<qreal>(0.00000001);

    if (path == QPainterPath() || pen == Qt::NoPen) return path;

    QPainterPathStroker ps;
    ps.setCapStyle(pen.capStyle());

    if (pen.widthF() <= 0.0)
        ps.setWidth(ZERO);
    else
        ps.setWidth(pen.widthF());

    ps.setJoinStyle(pen.joinStyle());
    ps.setMiterLimit(pen.miterLimit());

    QPainterPath p = ps.createStroke(path);
    p.addPath(path);

    return p;
}

static void highlight_selected(QPainter *painter, const ResizerF& geometry, bool border_anchor = false)
{
    const auto color = QColor("#969696");

    painter->setPen(QPen(color, 1, Qt::DashLine));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(geometry.rect());

    painter->setPen(QPen(color, 1.5, Qt::SolidLine));
    painter->setBrush(Qt::white);

    painter->drawEllipse(geometry.X1Y1Anchor());
    painter->drawEllipse(geometry.X1Y2Anchor());
    painter->drawEllipse(geometry.X2Y1Anchor());
    painter->drawEllipse(geometry.X2Y2Anchor());

    if (border_anchor && geometry.width() > 24 && geometry.height() > 24) {
        painter->drawEllipse(geometry.rightAnchor());
        painter->drawEllipse(geometry.leftAnchor());
        painter->drawEllipse(geometry.topAnchor());
        painter->drawEllipse(geometry.bottomAnchor());
    }

    if (geometry.rotationEnabled()) {
        painter->drawLine(geometry.rotateAnchor().center(), geometry.Y1Anchor().center());
        painter->drawEllipse(geometry.rotateAnchor());
    }
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

// pos at scene (0,0)
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

    pen_.setCapStyle(Qt::RoundCap);
    pen_.setJoinStyle(Qt::RoundJoin);

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

// pos at scene (0,0)
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

///

GraphicsRectItem::GraphicsRectItem(QGraphicsItem *parent)
    : GraphicsItem(parent)
{}

GraphicsRectItem::GraphicsRectItem(const QPointF& x1, const QPointF& x2, QGraphicsItem *parent)
    : GraphicsItem(parent)
{
    setVertexes(x1, x2);
}

// pos at scene (0,0)
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

QPainterPath GraphicsRectItem::shape() const
{
    QPainterPath path;
    if (filled()) {
        path.addRect(geometry_.boundingRect());
    }
    else {
        auto m = std::max(geometry_.borderWidth(), pen_.widthF()) / 2;
        path.addRect(geometry_.rect().adjusted(-m, -m, m, m));
        path.addRect(geometry_.rect().adjusted(m, m, -m, -m));
    }
    return path;
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

    pen_.setCapStyle(Qt::RoundCap);
    pen_.setJoinStyle(Qt::RoundJoin);

    if (filled()) {
        painter->fillRect(geometry_.rect(), pen_.color());
    }
    else {
        painter->setPen(pen_);
        painter->drawRect(geometry_.rect());
    }

    if (option->state & (QStyle::State_Selected | QStyle::State_HasFocus)) {
        highlight_selected(painter, geometry_, true);
    }
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

QPainterPath GraphicsEllipseleItem::shape() const
{
    QPainterPath path;
    if (filled()) {
        path.addRect(geometry_.boundingRect());
    }
    else {
        auto m = std::max(geometry_.borderWidth(), pen_.widthF()) / 2;

        path.addRect(geometry_.boundingRect().adjusted(-m, -m, m, m));
        path.addEllipse(geometry_.rect().adjusted(m, m, -m, -m));
    }
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

    painter->setPen(filled() ? QPen{ pen_.color(), 1 } : pen_);
    if (filled()) painter->setBrush(pen_.color());

    painter->drawEllipse(geometry_.rect());

    if (option->state & (QStyle::State_Selected | QStyle::State_HasFocus)) {
        highlight_selected(painter, geometry_, true);
    }
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

QPainterPath GraphicsPixmapItem::shape() const
{
    QPainterPath path;
    path.addRect(geometry_.boundingRect());
    return path;
}

void GraphicsPixmapItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    painter->drawPixmap(geometry_.rect(),
                        pixmap_.transformed(QTransform{}.scale(geometry_.hflipped() ? -1 : 1,
                                                               geometry_.vflipped() ? -1 : 1)),
                        QRectF{ {}, pixmap_.size() });

    if (option->state & (QStyle::State_Selected | QStyle::State_HasFocus)) {
        highlight_selected(painter, geometry_);
    }
}

void GraphicsPixmapItem::resize(const ResizerF& resizer, ResizerLocation location)
{
    auto oldrect = geometry_.rect().translated(-geometry_.topLeft());
    auto newrect = resizer.rect().translated(-resizer.topLeft());

    QSizeF newsize =
        newrect.contains(newrect.united(oldrect))
            ? QSizeF{ pixmap_.size() }.scaled(newrect.united(oldrect).size(), Qt::KeepAspectRatio)
            : QSizeF{ pixmap_.size() }.scaled(newrect.intersected(oldrect).size(), Qt::KeepAspectRatio);

    auto x1y1 = mapToScene(geometry_.x1(), geometry_.y1());
    auto x1y2 = mapToScene(geometry_.x1(), geometry_.y2());
    auto x2y1 = mapToScene(geometry_.x2(), geometry_.y1());
    auto x2y2 = mapToScene(geometry_.x2(), geometry_.y2());

    prepareGeometryChange();

    // fix me: v2 is on the topleft of v1
    geometry_.coords({ { 0, 0 }, newsize });
    geometry_.flip(resizer.hflipped(), resizer.vflipped());

    // update rotation origin point
    rotate(angle_);

    // clang-format off
    switch (location) {
    case ResizerLocation::X1Y1_ANCHOR: setPos(pos() + x2y2 - mapToScene(geometry_.x2(), geometry_.y2())); break;
    case ResizerLocation::X2Y1_ANCHOR: setPos(pos() + x1y2 - mapToScene(geometry_.x1(), geometry_.y2())); break;
    case ResizerLocation::X1Y2_ANCHOR: setPos(pos() + x2y1 - mapToScene(geometry_.x2(), geometry_.y1())); break;
    case ResizerLocation::X2Y2_ANCHOR: setPos(pos() + x1y1 - mapToScene(geometry_.x1(), geometry_.y1())); break;
    default: break;
    }
    // clang-format on

    update();
}

///

int GraphicsCounterleItem::counter = 0;

GraphicsCounterleItem::GraphicsCounterleItem(const QPointF& pos, int v, QGraphicsItem *parent)
    : GraphicsItem(parent)
{
    // dynamic caculate the rect
    QFont font;
    font.setPointSizeF(16);
    font.setBold(true);
    QFontMetrics metrics(font);
    QRectF four = metrics.boundingRect("44");
    qreal width = std::max(four.width(), four.height());

    //
    font_.setBold(true);

    geometry_ = ResizerF(QRectF{ 0, 0, width * 0.875, width * 0.875 }, width * 0.125);

    setPos(pos - geometry_.center());
    setCounter(v);
}

void GraphicsCounterleItem::setCounter(int v)
{
    counter_ = (v == -1) ? counter : v;
    counter  = counter_ + 1;
}

QPainterPath GraphicsCounterleItem::shape() const
{
    QPainterPath path;
    path.addRect(geometry_.boundingRect()); // to include corner anchors
    return shape_from_path(path, pen_);
}

ResizerLocation GraphicsCounterleItem::location(const QPointF&) const { return ResizerLocation::BORDER; }

void GraphicsCounterleItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    painter->setPen(QPen{ pen_.color() });
    painter->setBrush(pen_.color());

    painter->drawEllipse(geometry_.boundingRect());

    painter->setPen(QPen{ Qt::white });
    painter->setFont(font_);

    painter->drawText(geometry_.rect(), Qt::AlignCenter, QString::number(counter_));

    if (option->state & (QStyle::State_Selected | QStyle::State_HasFocus)) {
        auto color = QColor("#969696");

        painter->setPen(QPen(color, 1, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(geometry_.boundingRect());
    }
}

///

// TODO: smooth the curve
GraphicsPathItem::GraphicsPathItem(const QPointF& p, const QSizeF& size, QGraphicsItem *parent)
    : GraphicsItem(parent)
{
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setAcceptHoverEvents(false);
    setAcceptedMouseButtons(Qt::NoButton);

    vertexes_.push_back(p);
    tmpvtxes_.push_back(p);

    pixmap_ = QPixmap{ size.toSize() };
    pixmap_.fill(Qt::transparent);

    pen_.setCapStyle(Qt::RoundCap);
    pen_.setJoinStyle(Qt::RoundJoin);

    painter_ = new QPainter(&pixmap_);
    painter_->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    painter_->setCompositionMode(QPainter::CompositionMode_Source);
}

GraphicsPathItem::~GraphicsPathItem() { delete painter_; }

// for updating
QRectF GraphicsPathItem::boundingRect() const
{
    if (!fixed_) {
        QPainterPath path;
        path.addPolygon(tmpvtxes_);

        QPen pen = pen_;
        pen.setWidth(pen_.width() + 2);
        return shape_from_path(path, pen).controlPointRect();
    }
    else {
        return shape().controlPointRect();
    }
}

QPainterPath GraphicsPathItem::shape() const
{
    QPainterPath path;
    path.addPolygon(vertexes_);

    QPen pen = pen_;
    pen.setWidth(pen_.width() + 2);
    return shape_from_path(path, pen);
}

void GraphicsPathItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    if (!fixed_) {
        tmpvtxes_.clear();
        tmpvtxes_.push_back(vertexes_.back());
        painter->drawPixmap(shape().controlPointRect(), pixmap_, shape().controlPointRect());
    }
    else {
        painter->drawPixmap(pixmap_.rect(), pixmap_);
    }
}

void GraphicsPathItem::push(const QPointF& p)
{
    tmpvtxes_.push_back(p);

    painter_->drawLine(vertexes_.back(), p);

    prepareGeometryChange();
    vertexes_.push_back(p);
    update();
}

void GraphicsPathItem::setPen(const QPen& pen)
{
    if (pen_ == pen) return;

    pen_ = pen;
    pen_.setCapStyle(Qt::RoundCap);
    pen_.setJoinStyle(Qt::RoundJoin);
    painter_->setPen(pen_);

    redraw();
}

void GraphicsPathItem::redraw()
{
    pixmap_.fill(Qt::transparent);
    painter_->drawPolyline(vertexes_);

    prepareGeometryChange();
    tmpvtxes_ = vertexes_;
    update();
}

void GraphicsPathItem::end()
{
    fixed_ = true;

    redraw();
}

void GraphicsPathItem::focusOutEvent(QFocusEvent *event)
{
    setFlag(QGraphicsItem::ItemIsFocusable, false);

    if (!fixed_) {
        fixed_    = true;
        auto rect = shape().boundingRect().toRect().intersected(pixmap_.rect());
        pixmap_   = pixmap_.copy(rect);
        vertexes_ = mapFromScene(vertexes_);

        setPos(rect.topLeft());
    }

    GraphicsItem::focusOutEvent(event);
}

///

GraphicsCurveItem::GraphicsCurveItem(const QPointF& p, const QSizeF& size, QGraphicsItem *parent)
    : GraphicsPathItem(p, size, parent)
{}

///

GraphicsMosaicItem::GraphicsMosaicItem(const QPointF& p, const QSizeF& size, const QBrush& brush,
                                       QGraphicsItem *parent)
    : GraphicsPathItem(p, size, parent)
{
    pen_.setBrush(brush);
    painter_->setPen(pen_);
}

void GraphicsMosaicItem::setPen(const QPen& pen)
{
    if (pen_.widthF() == pen.widthF()) return;

    pen_.setWidthF(pen.widthF());
    painter_->setPen(pen_);

    redraw();
}

///

GraphicsEraserItem::GraphicsEraserItem(const QPointF& p, const QSizeF& size, const QBrush& brush,
                                       QGraphicsItem *parent)
    : GraphicsPathItem(p, size, parent)
{
    pen_.setBrush(brush);
    painter_->setPen(pen_);
}

void GraphicsEraserItem::setPen(const QPen& pen)
{
    if (pen_.widthF() == pen.widthF()) return;

    pen_.setWidth(pen.width());
    painter_->setPen(pen_);

    redraw();
}

///

GraphicsTextItem::GraphicsTextItem(const QPointF& pos, QGraphicsItem *parent)
    : GraphicsTextItem("", pos, parent)
{}

GraphicsTextItem::GraphicsTextItem(const QString& text, const QPointF& pos, QGraphicsItem *parent)
    : GraphicsItemWrapper(),
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

    QGraphicsTextItem::paint(painter, &text_render_option, nullptr);

    // render resizer
    if (option->state & (QStyle::State_Selected | QStyle::State_HasFocus)) {
        ResizerF geometry{ paddingRect() };
        geometry.enableRotate(true);
        highlight_selected(painter, geometry);
    }
}

void GraphicsTextItem::focusOutEvent(QFocusEvent *event)
{
    setTextInteractionFlags(Qt::NoTextInteraction);
    QGraphicsTextItem::focusOutEvent(event);
}

void GraphicsTextItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    GraphicsItemWrapper::hover(event);
    QGraphicsTextItem::hoverMoveEvent(event);
}

void GraphicsTextItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    GraphicsItemWrapper::hoverLeave(event);
    QGraphicsTextItem::hoverLeaveEvent(event);
}

void GraphicsTextItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        GraphicsItemWrapper::mousePress(event, pos());
    }
    else {
        setTextInteractionFlags(Qt::NoTextInteraction);
    }

    setFocus(Qt::MouseFocusReason);
    QGraphicsTextItem::mousePressEvent(event);
}

void GraphicsTextItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    switch (adjusting_) {
    case canvas::adjusting_t::moving: {
        auto offset = event->scenePos() - mpos_;
        moveBy(offset.x(), offset.y());
        mpos_ = event->scenePos();
        break;
    }
    case canvas::adjusting_t::rotating:
    case canvas::adjusting_t::resizing:
        GraphicsItemWrapper::mouseMove(event, mapToScene(QGraphicsTextItem::boundingRect().center()));
        break;

    default: QGraphicsTextItem::mouseMoveEvent(event); break;
    }
}

void GraphicsTextItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    GraphicsItemWrapper::mouseRelease(event, pos());
    QGraphicsTextItem::mouseReleaseEvent(event);
}

void GraphicsTextItem::resize(const ResizerF& resizer, ResizerLocation location)
{
    auto oldrect = QGraphicsTextItem::boundingRect();
    auto newrect = resizer.rect();

    QSizeF newsize = newrect.contains(newrect.united(oldrect))
                         ? oldrect.size().scaled(newrect.united(oldrect).size(), Qt::KeepAspectRatio)
                         : oldrect.size().scaled(newrect.intersected(oldrect).size(), Qt::KeepAspectRatio);

    auto scaled_fontsize = std::max(font().pointSizeF() * (newsize.width() / oldrect.width()), 5.0);

    if (scaled_fontsize == font().pointSizeF()) return;

    auto lt = mapToScene(oldrect.topLeft());
    auto lb = mapToScene(oldrect.bottomLeft());
    auto rt = mapToScene(oldrect.topRight());
    auto rb = mapToScene(oldrect.bottomRight());

    // resized font size
    auto scaledfont = font();
    scaledfont.setPointSizeF(scaled_fontsize);
    QGraphicsTextItem::setFont(scaledfont);

    // keep position after rotation
    geometry_.coords(QGraphicsTextItem::boundingRect());

    // update rotation origin point
    rotate(angle_);

    // position
    switch (location) {
    case ResizerLocation::X1Y1_ANCHOR: setPos(pos() + rb - mapToScene(geometry_.bottomRight())); break;
    case ResizerLocation::X2Y1_ANCHOR: setPos(pos() + lb - mapToScene(geometry_.bottomLeft())); break;
    case ResizerLocation::X1Y2_ANCHOR: setPos(pos() + rt - mapToScene(geometry_.topRight())); break;
    case ResizerLocation::X2Y2_ANCHOR: setPos(pos() + lt - mapToScene(geometry_.topLeft())); break;
    default: break;
    }
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