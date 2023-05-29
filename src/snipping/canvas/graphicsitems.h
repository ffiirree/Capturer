#ifndef CAPTURER_GRAPHICS_ITEMS_H
#define CAPTURER_GRAPHICS_ITEMS_H

#include "command.h"
#include "resizer.h"
#include "utils.h"

#include <QBrush>
#include <QGraphicsItem>
#include <QPen>

enum adjusting_t
{
    none,
    moving,
    resizing,
    rotating,
    editing, // text
};

class GraphicsItemInterface
{
public:
    virtual graph_t graph() const = 0;

    // getter
    virtual QPen pen() const { return Qt::NoPen; }

    virtual QBrush brush() const { return Qt::NoBrush; }

    virtual bool filled() const { return false; }

    virtual QFont font() const { return {}; }

    // setter
    virtual void setPen(const QPen&) {}

    virtual void setBrush(const QBrush&) {}

    virtual void setFont(const QFont&) {}

    virtual void fill(bool) {}

    // push or update points
    virtual void push(const QPointF&) = 0;

    //
    virtual bool invalid() const = 0;

    //
    virtual Resizer::PointPosition location(const QPointF&) const = 0;

    // callbacks
    virtual void onhovered(const std::function<void(Resizer::PointPosition)>& fn) { onhover = fn; }

    virtual void onfocused(const std::function<void()>& fn) { onfocus = fn; }

    virtual void onblurred(const std::function<void()>& fn) { onblur = fn; }

    virtual void onchanged(const std::function<void(const std::shared_ptr<Command>&)>& fn)
    {
        onchange = fn;
    }

protected:
    std::function<void(Resizer::PointPosition)> onhover           = [](auto) {};
    std::function<void()> onfocus                                 = []() {};
    std::function<void()> onblur                                  = []() {};
    std::function<void(const std::shared_ptr<Command>&)> onchange = [](auto) {};

    Resizer::PointPosition hover_location_ = Resizer::DEFAULT;
    adjusting_t adjusting_                 = adjusting_t::none;
    qreal adjusting_min_w_                 = 12.0;
};

class GraphicsItem : public GraphicsItemInterface, public QGraphicsItem
{
public:
    explicit GraphicsItem(QGraphicsItem * = nullptr);

protected:
    void focusInEvent(QFocusEvent *) override;
    void focusOutEvent(QFocusEvent *) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *) override;
    void keyPressEvent(QKeyEvent *) override;

    void mousePressEvent(QGraphicsSceneMouseEvent *) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *) override;
};

/// GraphicsLineItem

class GraphicsLineItem : public GraphicsItem
{
public:
    explicit GraphicsLineItem(QGraphicsItem * = nullptr);
    explicit GraphicsLineItem(const QPointF&, const QPointF&, QGraphicsItem * = nullptr);

    ~GraphicsLineItem() {}

    // QGraphicsItem
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    bool contains(const QPointF&) const override;

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget * = nullptr) override;

    //
    void setVertexes(const QPointF&, const QPointF&);

    // GraphicsItemInterface
    graph_t graph() const override { return graph_t::line; }

    QPen pen() const override;

    void setPen(const QPen&) override;

    bool invalid() const override { return QLineF(v1_, v2_).length() < 4; }

    void push(const QPointF&) override;

    Resizer::PointPosition location(const QPointF&) const override;

protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent *) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *) override;

private:
    QPointF v1_{};
    QPointF v2_{};

    QPen pen_{ Qt::red, 3, Qt::SolidLine };
};

/// GraphicsArrowItem

class GraphicsArrowItem : public GraphicsItem
{
public:
    explicit GraphicsArrowItem(QGraphicsItem * = nullptr);
    explicit GraphicsArrowItem(const QPointF&, const QPointF&, QGraphicsItem * = nullptr);

    ~GraphicsArrowItem() {}

    // QGraphicsItem
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    bool contains(const QPointF&) const override;

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget * = nullptr) override;

    //
    void setVertexes(const QPointF&, const QPointF&);

    // GraphicsItemInterface
    QPen pen() const override;

    void setPen(const QPen&) override;

    graph_t graph() const override { return graph_t::arrow; }

    void push(const QPointF&) override;

    bool invalid() const override { return QLineF(polygon_[0], polygon_[3]).length() < 25; }

    Resizer::PointPosition location(const QPointF&) const override;

protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent *) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *) override;

private:
    QPolygonF polygon_{ 6 };
    QPen pen_{ Qt::red, 1, Qt::SolidLine };
};

/// GraphicsRectItem

class GraphicsRectItem : public GraphicsItem
{
public:
    explicit GraphicsRectItem(QGraphicsItem * = nullptr);
    explicit GraphicsRectItem(const QPointF&, const QPointF&, QGraphicsItem * = nullptr);

    ~GraphicsRectItem() {}

    // QGraphicsItem
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    bool contains(const QPointF&) const override;

    bool filled() const override;
    void fill(bool) override;

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget * = nullptr) override;

    //
    void setVertexes(const QPointF&, const QPointF&);

    // GraphicsItemInterface
    graph_t graph() const override { return graph_t::rectangle; }

    QPen pen() const override;
    void setPen(const QPen&) override;

    QBrush brush() const override;
    void setBrush(const QBrush&) override;

    void push(const QPointF&) override;

    bool invalid() const override { return std::abs(v2_.x() - v1_.x()) * std::abs(v2_.y() - v1_.y()) < 16; }

    Resizer::PointPosition location(const QPointF&) const override;

protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent *) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *) override;

private:
    QPointF v1_{};
    QPointF v2_{};

    QPen pen_{ Qt::red, 3, Qt::SolidLine };
    QBrush brush_{ Qt::NoBrush };
    bool filled_{};
};

/// GraphicsEllipseleItem

class GraphicsEllipseleItem : public GraphicsItem
{
public:
    explicit GraphicsEllipseleItem(QGraphicsItem * = nullptr);
    explicit GraphicsEllipseleItem(const QPointF&, const QPointF&, QGraphicsItem * = nullptr);

    ~GraphicsEllipseleItem() {}

    // QGraphicsItem
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    bool contains(const QPointF&) const override;

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget * = nullptr) override;

    //
    void setVertexes(const QPointF&, const QPointF&);

    // GraphicsItemInterface
    graph_t graph() const override { return graph_t::ellipse; }

    QPen pen() const override;
    void setPen(const QPen&) override;

    QBrush brush() const override;
    void setBrush(const QBrush&) override;

    bool filled() const override;
    void fill(bool) override;

    void push(const QPointF&) override;

    bool invalid() const override { return std::abs(v2_.x() - v1_.x()) * std::abs(v2_.y() - v1_.y()) < 16; }

    Resizer::PointPosition location(const QPointF&) const override;

protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent *) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *) override;

private:
    QPointF v1_{};
    QPointF v2_{};

    QPen pen_{ Qt::red, 3, Qt::SolidLine };
    QBrush brush_{ Qt::NoBrush };
    bool filled_{};
};

/// GraphicsPathItem
class GraphicsPathItem : public GraphicsItem
{
public:
    explicit GraphicsPathItem(const QPointF&, QGraphicsItem * = nullptr);

    ~GraphicsPathItem() {}

    // QGraphicsItem
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    bool contains(const QPointF&) const override;

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget * = nullptr) override;

    //
    void pushVertexes(const QVector<QPointF>&);

    // GraphicsItemInterface
    graph_t graph() const override { return graph_t::curve; }

    QPen pen() const override;
    void setPen(const QPen&) override;

    void push(const QPointF&) override;

    bool invalid() const override { return false; }

    Resizer::PointPosition location(const QPointF&) const override { return Resizer::DEFAULT; }

private:
    QPainterPath path_{};
    QPen pen_{ Qt::red, 10, Qt::SolidLine };
};

class GraphicsCurveItem : public GraphicsPathItem
{
public:
    explicit GraphicsCurveItem(const QPointF&, QGraphicsItem * = nullptr);

    graph_t graph() const override { return graph_t::curve; }
};

class GraphicsMosaicItem : public GraphicsPathItem
{
public:
    explicit GraphicsMosaicItem(const QPointF&, QGraphicsItem * = nullptr);

    graph_t graph() const override { return graph_t::mosaic; }
};

class GraphicsEraserItem : public GraphicsPathItem
{
public:
    explicit GraphicsEraserItem(const QPointF&, QGraphicsItem * = nullptr);

    graph_t graph() const override { return graph_t::eraser; }
};

///

class GraphicsTextItem : public GraphicsItemInterface, public QGraphicsTextItem
{
public:
    explicit GraphicsTextItem(const QPointF&, QGraphicsItem * = nullptr);
    explicit GraphicsTextItem(const QString&, const QPointF&, QGraphicsItem * = nullptr);

    // QGraphicsTextItem
    QRectF boundingRect() const override;
    QPainterPath shape() const override;

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget * = nullptr) override;

    // GraphicsItemInterface
    graph_t graph() const override { return graph_t::text; }

    QPen pen() const override { return QPen(defaultTextColor()); }

    void setPen(const QPen& pen) override { setDefaultTextColor(pen.color()); }

    QFont font() const override { return QGraphicsTextItem::font(); }

    void setFont(const QFont& f) override { QGraphicsTextItem::setFont(f); }

    void push(const QPointF&) override {}

    bool invalid() const override { return toPlainText().isEmpty(); }

    Resizer::PointPosition location(const QPointF&) const override;

    //
    QRectF paddingRect() const;

protected:
    void focusInEvent(QFocusEvent *) override;
    void focusOutEvent(QFocusEvent *) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *) override;

private:
    const float padding = 5;
};

#endif //! CAPTURER_GRAPHICS_ITEMS_H