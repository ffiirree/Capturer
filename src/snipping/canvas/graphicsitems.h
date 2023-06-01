#ifndef CAPTURER_GRAPHICS_ITEMS_H
#define CAPTURER_GRAPHICS_ITEMS_H

#include "command.h"
#include "resizer.h"
#include "types.h"

#include <QBrush>
#include <QGraphicsItem>
#include <QPen>

class GraphicsItemInterface
{
public:
    virtual canvas::graphics_t graph() const = 0;

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
    virtual ResizerLocation location(const QPointF&) const = 0;

    //
    virtual void rotate(qreal) {}

    //
    virtual QGraphicsItem *copy() const = 0;

    virtual void resize(const ResizerF&, ResizerLocation) {}

    // callbacks
    virtual void onhovered(const std::function<void(ResizerLocation)>& fn) { onhover = fn; }

    virtual void onfocused(const std::function<void()>& fn) { onfocus = fn; }

    virtual void onblurred(const std::function<void()>& fn) { onblur = fn; }

    virtual void onchanged(const std::function<void(const std::shared_ptr<Command>&)>& fn)
    {
        onchange = fn;
    }

    virtual void onresized(const std::function<void(const ResizerF&)>& fn) { onresize = fn; }

protected:
    // callbacks
    std::function<void(ResizerLocation)> onhover                  = [](auto) {};
    std::function<void()> onfocus                                 = []() {};
    std::function<void()> onblur                                  = []() {};
    std::function<void(const std::shared_ptr<Command>&)> onchange = [](auto) {};
    std::function<void(const ResizerF&)> onresize                 = [](auto) {};

    // hovering
    ResizerLocation hover_location_ = ResizerLocation::DEFAULT;

    //
    canvas::adjusting_t adjusting_ = canvas::adjusting_t::none;
    qreal adjusting_min_w_         = 12.0;

    // rotation
    qreal angle_{};

    // used by moving / rotating
    QPointF mpos_{};

    // for recording adjusted command
    ResizerF before_resizing_{}; // rectangle
    qreal before_rotating_{};    // angle
    QPointF before_moving_{};    // position
};

class GraphicsItem : public GraphicsItemInterface, public QGraphicsItem
{
public:
    explicit GraphicsItem(QGraphicsItem * = nullptr);

protected:
    void focusInEvent(QFocusEvent *) override;
    void focusOutEvent(QFocusEvent *) override;
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
    canvas::graphics_t graph() const override { return canvas::graphics_t::line; }

    QPen pen() const override;

    void setPen(const QPen&) override;

    bool invalid() const override { return QLineF(v1_, v2_).length() < 4; }

    void push(const QPointF&) override;

    ResizerLocation location(const QPointF&) const override;

    //
    void resize(const ResizerF&, ResizerLocation) override;

    QGraphicsItem *copy() const override;

protected:
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

    canvas::graphics_t graph() const override { return canvas::graphics_t::arrow; }

    void push(const QPointF&) override;

    bool invalid() const override { return QLineF(polygon_[0], polygon_[3]).length() < 25; }

    ResizerLocation location(const QPointF&) const override;

    void resize(const ResizerF&, ResizerLocation) override;

    QGraphicsItem *copy() const;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *) override;

private:
    QPolygonF polygon_{ 6 };
    QPen pen_{ Qt::red, 1, Qt::SolidLine };
};

/// Rectangle Item

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
    canvas::graphics_t graph() const override { return canvas::graphics_t::rectangle; }

    QPen pen() const override;
    void setPen(const QPen&) override;

    QBrush brush() const override;
    void setBrush(const QBrush&) override;

    void push(const QPointF&) override;

    bool invalid() const override { return std::abs(v2_.x() - v1_.x()) * std::abs(v2_.y() - v1_.y()) < 16; }

    ResizerLocation location(const QPointF&) const override;

    void resize(const ResizerF& g, ResizerLocation) override;

    QGraphicsItem *copy() const override;

protected:
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

/// Pixmap Item

class GraphicsPixmapItem : public GraphicsItem
{
public:
    explicit GraphicsPixmapItem(const QPixmap&, const QPointF& center, QGraphicsItem * = nullptr);

    ~GraphicsPixmapItem() {}

    // QGraphicsItem
    QRectF boundingRect() const override;
    QPainterPath shape() const override;

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget * = nullptr) override;

    //
    void setPixmap(const QPixmap&);

    // GraphicsItemInterface
    canvas::graphics_t graph() const override { return canvas::graphics_t::pixmap; }

    void push(const QPointF&) override {}

    bool filled() const override { return true; }

    bool invalid() const override { return pixmap_.size() == QSize{ 0, 0 }; }

    ResizerLocation location(const QPointF&) const override;

    void resize(const ResizerF& g, ResizerLocation) override;

    void rotate(qreal) override;

    QGraphicsItem *copy() const override;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *) override;

private:
    QPointF v1_{};
    QPointF v2_{};

    QPixmap pixmap_{};
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
    canvas::graphics_t graph() const override { return canvas::graphics_t::ellipse; }

    QPen pen() const override;
    void setPen(const QPen&) override;

    QBrush brush() const override;
    void setBrush(const QBrush&) override;

    bool filled() const override;
    void fill(bool) override;

    void push(const QPointF&) override;

    bool invalid() const override { return std::abs(v2_.x() - v1_.x()) * std::abs(v2_.y() - v1_.y()) < 16; }

    ResizerLocation location(const QPointF&) const override;

    void resize(const ResizerF&, ResizerLocation) override;

    QGraphicsItem *copy() const override;

protected:
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

/// Counter Item

class GraphicsCounterleItem : public GraphicsItem
{
public:
    explicit GraphicsCounterleItem(const QPointF&, int, QGraphicsItem * = nullptr);

    ~GraphicsCounterleItem() {}

    // QGraphicsItem
    QRectF boundingRect() const override;
    QPainterPath shape() const override;

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget * = nullptr) override;

    //
    void setCounter(int);

    // GraphicsItemInterface
    canvas::graphics_t graph() const override { return canvas::graphics_t::counter; }

    // QPen pen() const override;
    // void setPen(const QPen&) override;

    bool filled() const override { return true; }

    void push(const QPointF&) override {}

    bool invalid() const override { return false; }

    ResizerLocation location(const QPointF&) const override;

    // void resize(const ResizerF&, ResizerLocation) override;

    // disable copy
    QGraphicsItem *copy() const { return nullptr; }

private:
    QPointF v1_{};
    QPointF v2_{};

    QPen pen_{ Qt::red, 3, Qt::SolidLine };

    int counter_{};

    static int counter;
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
    canvas::graphics_t graph() const override { return canvas::graphics_t::curve; }

    QPen pen() const override;
    void setPen(const QPen&) override;

    void push(const QPointF&) override;

    bool invalid() const override { return false; }

    ResizerLocation location(const QPointF&) const override { return ResizerLocation::DEFAULT; }

    QGraphicsItem *copy() const { return nullptr; }

private:
    QPainterPath path_{};
    QPen pen_{ Qt::red, 10, Qt::SolidLine };
};

class GraphicsCurveItem : public GraphicsPathItem
{
public:
    explicit GraphicsCurveItem(const QPointF&, QGraphicsItem * = nullptr);

    canvas::graphics_t graph() const override { return canvas::graphics_t::curve; }
};

class GraphicsMosaicItem : public GraphicsPathItem
{
public:
    explicit GraphicsMosaicItem(const QPointF&, QGraphicsItem * = nullptr);

    canvas::graphics_t graph() const override { return canvas::graphics_t::mosaic; }
};

class GraphicsEraserItem : public GraphicsPathItem
{
public:
    explicit GraphicsEraserItem(const QPointF&, QGraphicsItem * = nullptr);

    canvas::graphics_t graph() const override { return canvas::graphics_t::eraser; }
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
    canvas::graphics_t graph() const override { return canvas::graphics_t::text; }

    QPen pen() const override { return QPen(defaultTextColor()); }

    void setPen(const QPen& pen) override { setDefaultTextColor(pen.color()); }

    QFont font() const override { return QGraphicsTextItem::font(); }

    void setFont(const QFont& f) override { QGraphicsTextItem::setFont(f); }

    void push(const QPointF&) override {}

    bool invalid() const override { return toPlainText().isEmpty(); }

    ResizerLocation location(const QPointF&) const override;

    //
    QRectF paddingRect() const;

    //
    void resize(const ResizerF&, ResizerLocation) override;

    void rotate(qreal) override;

    QGraphicsItem *copy() const override;

protected:
    void focusInEvent(QFocusEvent *) override;
    void focusOutEvent(QFocusEvent *) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *) override;

private:
    const float padding = 5;
};

#endif //! CAPTURER_GRAPHICS_ITEMS_H