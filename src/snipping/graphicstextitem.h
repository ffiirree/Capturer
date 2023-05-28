#ifndef CAPTURER_GRAPHICS_TEXT_ITEM_H
#define CAPTURER_GRAPHICS_TEXT_ITEM_H

#include "command.h"
#include "resizer.h"
#include "utils.h"

#include <QGraphicsTextItem>
#include <QPoint>

class GraphicsTextItem : public QGraphicsTextItem
{
public:
    GraphicsTextItem(const QString&, const QPointF& vs);

    Resizer::PointPosition location(const QPoint&);
    std::function<void(Resizer::PointPosition)> onhovered;
    std::function<void()> ondeleted;
    std::function<void()> onresized;
    std::function<void(const QPoint&)> onmoved;
    std::function<void()> onfocus;
    std::function<void()> onblur;

signals:
    void changed(const std::shared_ptr<Command>&);

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
    QPointF spos_{};
};

#endif //! CAPTURER_GRAPHICS_TEXT_ITEM_H