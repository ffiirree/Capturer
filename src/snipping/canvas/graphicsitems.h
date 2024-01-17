#ifndef CAPTURER_GRAPHICS_ITEMS_H
#define CAPTURER_GRAPHICS_ITEMS_H

#include "command.h"
#include "resizer.h"
#include "types.h"

#include <QGraphicsItem>
#include <QPen>

class GraphicsItemWrapper
{
public:
    virtual ~GraphicsItemWrapper() = default;

    [[nodiscard]] virtual canvas::graphics_t graph() const = 0;

    // getter
    [[nodiscard]] virtual QPen pen() const { return Qt::NoPen; }

    [[nodiscard]] virtual bool filled() const { return false; }

    [[nodiscard]] virtual QFont font() const { return {}; }

    // setter
    virtual void setPen(const QPen&) {}

    virtual void setFont(const QFont&) {}

    virtual void fill(bool) {}

    // push or update points
    virtual void push(const QPointF&) = 0;

    //
    [[nodiscard]] virtual bool invalid() const = 0;

    [[nodiscard]] virtual bool creating() const { return false; }

    //
    [[nodiscard]] virtual ResizerLocation location(const QPointF&) const;

    // rotate
    [[nodiscard]] virtual qreal angle() const { return angle_; }

    virtual void rotate(qreal);

    // resize
    [[nodiscard]] virtual ResizerF geometry() const { return geometry_; }

    virtual void resize(const ResizerF&, ResizerLocation) {}

    //
    virtual void end(){};

    // callbacks
    virtual void onhovered(const std::function<void(ResizerLocation)>& fn) { onhover = fn; }

    virtual void onresized(const std::function<void(const ResizerF&, ResizerLocation)>& fn)
    {
        onresize = fn;
    }

    virtual void onmoved(const std::function<void(const QPointF&)>& fn) { onmove = fn; }

    virtual void onrotated(const std::function<void(qreal)>& fn) { onrotate = fn; }

protected:
    // events
    void hover(QGraphicsSceneHoverEvent *);
    void hoverLeave(QGraphicsSceneHoverEvent *);

    void mousePress(QGraphicsSceneMouseEvent *, const QPointF& pos);
    void mouseMove(QGraphicsSceneMouseEvent *, const QPointF& center);
    void mouseRelease(QGraphicsSceneMouseEvent *, const QPointF& pos);

    // callbacks
    std::function<void(ResizerLocation)> onhover                   = [](auto) {};
    std::function<void(const ResizerF&, ResizerLocation)> onresize = [](auto, auto) {};
    std::function<void(const QPointF&)> onmove                     = [](auto) {};
    std::function<void(qreal)> onrotate                            = [](auto) {};

    // hovering
    ResizerLocation hover_location_ = ResizerLocation::DEFAULT;

    //
    canvas::adjusting_t adjusting_ = canvas::adjusting_t::none;

    // rotation
    qreal angle_{};

    // used by moving / rotating
    QPointF mpos_{};

    //
    ResizerF geometry_{ 0.0, 0.0, 0.0, 0.0, 12.0 };

    // for recording adjusted command
    ResizerF before_resizing_{}; // geometry
    qreal before_rotating_{};    // angle
    QPointF before_moving_{};    // position
};

class GraphicsItem : public GraphicsItemWrapper, public QGraphicsItem
{
public:
    explicit GraphicsItem(QGraphicsItem * = nullptr);

    [[nodiscard]] QRectF boundingRect() const override;

    //
    void resize(const ResizerF&, ResizerLocation) override;

    // getter
    [[nodiscard]] QPen pen() const override;

    // setter
    void setPen(const QPen&) override;

protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent *) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *) override;

    void mousePressEvent(QGraphicsSceneMouseEvent *) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *) override;

    QPen pen_{ Qt::red, 3, Qt::SolidLine };
};

/// Line Item

class GraphicsLineItem : public GraphicsItem
{
public:
    explicit GraphicsLineItem(QGraphicsItem * = nullptr);
    explicit GraphicsLineItem(const QPointF&, const QPointF&, QGraphicsItem * = nullptr);

    // QGraphicsItem
    [[nodiscard]] QPainterPath shape() const override;

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget * = nullptr) override;

    //
    void setVertexes(const QPointF&, const QPointF&);

    // GraphicsItemWrapper
    [[nodiscard]] canvas::graphics_t graph() const override { return canvas::graphics_t::line; }

    [[nodiscard]] bool invalid() const override
    {
        return QLineF(geometry_.point1(), geometry_.point2()).length() < 4;
    }

    void push(const QPointF&) override;

    [[nodiscard]] ResizerLocation location(const QPointF&) const override;
};

/// Arrow Item

class GraphicsArrowItem : public GraphicsItem
{
public:
    explicit GraphicsArrowItem(QGraphicsItem * = nullptr);
    explicit GraphicsArrowItem(const QPointF&, const QPointF&, QGraphicsItem * = nullptr);

    // QGraphicsItem
    [[nodiscard]] QPainterPath shape() const override;

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget * = nullptr) override;

    //
    void setVertexes(const QPointF&, const QPointF&);

    [[nodiscard]] canvas::graphics_t graph() const override { return canvas::graphics_t::arrow; }

    void push(const QPointF&) override;

    [[nodiscard]] bool invalid() const override { return QLineF(polygon_[0], polygon_[3]).length() < 25; }

    [[nodiscard]] ResizerLocation location(const QPointF&) const override;

    void resize(const ResizerF&, ResizerLocation) override;

private:
    QPolygonF polygon_{ 6 };
};

/// Rectangle Item

class GraphicsRectItem : public GraphicsItem
{
public:
    explicit GraphicsRectItem(QGraphicsItem * = nullptr);
    explicit GraphicsRectItem(const QPointF&, const QPointF&, QGraphicsItem * = nullptr);

    // QGraphicsItem
    [[nodiscard]] QPainterPath shape() const override;

    [[nodiscard]] bool filled() const override;
    void fill(bool) override;

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget * = nullptr) override;

    //
    void setVertexes(const QPointF&, const QPointF&);

    // GraphicsItemWrapper
    [[nodiscard]] canvas::graphics_t graph() const override { return canvas::graphics_t::rectangle; }

    void push(const QPointF&) override;

    [[nodiscard]] bool invalid() const override { return geometry_.width() * geometry_.height() < 16; }

private:
    bool filled_{};
};

/// Pixmap Item

class GraphicsPixmapItem : public GraphicsItem
{
public:
    explicit GraphicsPixmapItem(const QPixmap&, const QPointF&, QGraphicsItem * = nullptr);

    // QGraphicsItem
    [[nodiscard]] QPainterPath shape() const override;

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget * = nullptr) override;

    //
    void setPixmap(const QPixmap&);

    // GraphicsItemWrapper
    [[nodiscard]] canvas::graphics_t graph() const override { return canvas::graphics_t::pixmap; }

    void push(const QPointF&) override {}

    [[nodiscard]] bool filled() const override { return true; }

    [[nodiscard]] bool invalid() const override { return pixmap_.size() == QSize{ 0, 0 }; }

    void resize(const ResizerF& g, ResizerLocation) override;

private:
    QPixmap pixmap_{};
};

/// Ellipse Item

class GraphicsEllipseleItem : public GraphicsItem
{
public:
    explicit GraphicsEllipseleItem(QGraphicsItem * = nullptr);
    explicit GraphicsEllipseleItem(const QPointF&, const QPointF&, QGraphicsItem * = nullptr);

    // QGraphicsItem
    [[nodiscard]] QPainterPath shape() const override;

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget * = nullptr) override;

    //
    void setVertexes(const QPointF&, const QPointF&);

    // GraphicsItemWrapper
    [[nodiscard]] canvas::graphics_t graph() const override { return canvas::graphics_t::ellipse; }

    [[nodiscard]] bool filled() const override;
    void fill(bool) override;

    void push(const QPointF&) override;

    bool invalid() const override { return geometry_.width() * geometry_.height() < 16; }

    [[nodiscard]] ResizerLocation location(const QPointF&) const override;

private:
    bool filled_{};
};

/// Counter Item

class GraphicsCounterleItem : public GraphicsItem
{
public:
    explicit GraphicsCounterleItem(const QPointF&, int, QGraphicsItem * = nullptr);

    // QGraphicsItem
    [[nodiscard]] QPainterPath shape() const override;

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget * = nullptr) override;

    //
    void setCounter(int);

    // GraphicsItemWrapper
    [[nodiscard]] canvas::graphics_t graph() const override { return canvas::graphics_t::counter; }

    [[nodiscard]] bool filled() const override { return true; }

    void push(const QPointF&) override {}

    [[nodiscard]] bool invalid() const override { return false; }

    [[nodiscard]] ResizerLocation location(const QPointF&) const override;

private:
    int counter_{};

    static int counter;
    QFont font_{};
};

/// Path Item

class GraphicsPathItem : public GraphicsItem
{
public:
    explicit GraphicsPathItem(const QPointF&, const QSizeF& size, QGraphicsItem * = nullptr);

    ~GraphicsPathItem() override;

    // QGraphicsItem
    [[nodiscard]] QRectF boundingRect() const override;
    [[nodiscard]] QPainterPath shape() const override;

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget * = nullptr) override;

    // GraphicsItemWrapper
    [[nodiscard]] canvas::graphics_t graph() const override { return canvas::graphics_t::curve; }

    void push(const QPointF&) override;

    void setPen(const QPen&) override;

    [[nodiscard]] bool invalid() const override { return false; }

    [[nodiscard]] ResizerLocation location(const QPointF&) const override
    {
        return ResizerLocation::DEFAULT;
    }

    void end() override;

    //
    virtual void redraw();

protected:
    void focusOutEvent(QFocusEvent *) override;

    QVector<QPointF> vertexes_{};

    QVector<QPointF> tmpvtxes_{};
    bool fixed_{};

    QPixmap pixmap_{};
    QPainter *painter_{};
};

class GraphicsCurveItem : public GraphicsPathItem
{
public:
    explicit GraphicsCurveItem(const QPointF&, const QSizeF& size, QGraphicsItem * = nullptr);

    [[nodiscard]] canvas::graphics_t graph() const override { return canvas::graphics_t::curve; }
};

class GraphicsMosaicItem : public GraphicsPathItem
{
public:
    explicit GraphicsMosaicItem(const QPointF&, const QSizeF& size, const QBrush& brush,
                                QGraphicsItem * = nullptr);

    void setPen(const QPen&) override;

    [[nodiscard]] canvas::graphics_t graph() const override { return canvas::graphics_t::mosaic; }
};

class GraphicsEraserItem : public GraphicsPathItem
{
public:
    explicit GraphicsEraserItem(const QPointF&, const QSizeF& size, const QBrush& brush,
                                QGraphicsItem * = nullptr);

    void setPen(const QPen&) override;

    [[nodiscard]] canvas::graphics_t graph() const override { return canvas::graphics_t::eraser; }
};

/// Text Item

class GraphicsTextItem : public GraphicsItemWrapper, public QGraphicsTextItem
{
public:
    explicit GraphicsTextItem(const QPointF&, QGraphicsItem * = nullptr);
    explicit GraphicsTextItem(const QString&, const QPointF&, QGraphicsItem * = nullptr);

    // QGraphicsTextItem
    [[nodiscard]] QRectF boundingRect() const override;
    [[nodiscard]] QPainterPath shape() const override;

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget * = nullptr) override;

    // GraphicsItemWrapper
    [[nodiscard]] canvas::graphics_t graph() const override { return canvas::graphics_t::text; }

    [[nodiscard]] QPen pen() const override { return QPen{ defaultTextColor() }; }

    void setPen(const QPen& pen) override { setDefaultTextColor(pen.color()); }

    [[nodiscard]] QFont font() const override { return QGraphicsTextItem::font(); }

    void setFont(const QFont&) override;

    void push(const QPointF&) override {}

    [[nodiscard]] bool invalid() const override { return toPlainText().isEmpty(); }

    [[nodiscard]] ResizerLocation location(const QPointF&) const override;

    //
    [[nodiscard]] QRectF paddingRect() const;

    //
    void resize(const ResizerF&, ResizerLocation) override;

    [[nodiscard]] ResizerF geometry() const override
    {
        return ResizerF{ QGraphicsTextItem::boundingRect() };
    }

protected:
    void focusOutEvent(QFocusEvent *) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *) override;

private:
    const float padding = 5;
};

#endif //! CAPTURER_GRAPHICS_ITEMS_H