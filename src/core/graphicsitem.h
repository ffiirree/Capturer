#ifndef CAPTURER_GRAPHICS_ITEM_H
#define CAPTURER_GRAPHICS_ITEM_H

#include "utils.h"
#include "command.h"

#include <QGraphicsItem>
#include <QPoint>

class GraphicsItem : public QGraphicsItem
{
public:
    GraphicsItem(graph_t, const QVector<QPointF>&);

    QVector<QPointF> vertexes() const { return vertexes_; }

    void pushVertex(const QPointF&);

    QRectF boundingRect() const override;

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) override;

signals:
    void changed(const std::shared_ptr<Command>&);

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
    QPointF spos_{};
};


#endif