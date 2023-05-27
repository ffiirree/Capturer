#ifndef CAPTURER_GRAPHICS_ITEM_H
#define CAPTURER_GRAPHICS_ITEM_H

#include "command.h"
#include "utils.h"
#include "resizer.h"

#include <QBrush>
#include <QGraphicsItem>
#include <QPen>
#include <QPoint>
#include <functional>

class GraphicsItem : public QGraphicsItem
{
public:
    enum
    {
        NORMAL,
        SHIELDED,
        MOVING,
        RESIZING,
    };

public:
    GraphicsItem(graph_t, const QVector<QPointF>&);

    QVector<QPointF> vertexes() const { return vertexes_; }

    void pushVertex(const QPointF&);

    QRectF boundingRect() const override;

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) override;

    QPen pen() const { return pen_; }

    QBrush brush() const { return brush_; }

    void setPen(const QPen& pen)
    {
        pen_ = pen;
        update();
    }

    void setBrush(const QBrush& brush)
    {
        brush_ = brush;
        update();
    }

    Resizer::PointPosition location(const QPoint&);

    bool invalid();

    std::function<void(Resizer::PointPosition)> onhovered;
    std::function<void()> ondeleted;
    std::function<void()> onresized;
    std::function<void(const QPoint&)> onmoved;

protected:
    void focusInEvent(QFocusEvent *);
    void focusOutEvent(QFocusEvent *);
    void hoverEnterEvent(QGraphicsSceneHoverEvent *);
    void hoverMoveEvent(QGraphicsSceneHoverEvent *);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *);
    void keyPressEvent(QKeyEvent *);
    void keyReleaseEvent(QKeyEvent *);
    void mousePressEvent(QGraphicsSceneMouseEvent *);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *);

private:
    QVector<QPointF> vertexes_{};

    graph_t graph_{};

    // resizing
    Resizer::PointPosition hover_location_{};
    Resizer::PointPosition location_{};
    QPointF spos_{};

    QPen pen_{};
    QBrush brush_{};

    uint32_t status_{};
};

#endif