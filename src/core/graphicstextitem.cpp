#include "graphicstextitem.h"

#include "logging.h"
#include "resizer.h"

#include <numbers>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>

inline std::ostream& operator<<(std::ostream& out, const QRectF& rect)
{
    return out << "<<" << rect.left() << ", " << rect.top() << ">, <" << rect.width() << " x "
               << rect.height() << ">>";
}

GraphicsTextItem::GraphicsTextItem(const QString& text, const QPointF& vs)
    : QGraphicsTextItem(text)
{
    setFlag(QGraphicsItem::ItemIsFocusable);
    setFlag(QGraphicsItem::ItemIsMovable);
    setAcceptHoverEvents(true);

    setPos(vs.x(), vs.y());
}

void GraphicsTextItem::focusInEvent(QFocusEvent *)
{
    LOG(INFO) << "TextItem focusInEvent: " << boundingRect();
}

void GraphicsTextItem::focusOutEvent(QFocusEvent *)
{
    LOG(INFO) << "TextItem focusOutEvent: " << boundingRect();
    setTextInteractionFlags(Qt::NoTextInteraction);
}

void GraphicsTextItem::hoverEnterEvent(QGraphicsSceneHoverEvent *)
{
    LOG(INFO) << "TextItem hoverEnterEvent: " << boundingRect();
}

void GraphicsTextItem::hoverMoveEvent(QGraphicsSceneHoverEvent *)
{
    // LOG(INFO) << "hoverMoveEvent: " << boundingRect();
}

void GraphicsTextItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *)
{
    LOG(INFO) << "TextItem hoverLeaveEvent: " << boundingRect();
}

void GraphicsTextItem::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete) {
        setVisible(false);
    }
    LOG(INFO) << "TextItem keyPressEvent";
}

void GraphicsTextItem::keyReleaseEvent(QKeyEvent *) { LOG(INFO) << "TextItem keyReleaseEvent"; }

void GraphicsTextItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    spos_ = event->pos();

    LOG(INFO) << "TextItem mousePressEvent";
    QGraphicsTextItem::mousePressEvent(event);
}

void GraphicsTextItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    auto offset = event->pos() - spos_;
    // moveBy(offset.x(), offset.y());

    //LOG(INFO) << "TextItem mouseMoveEvent: (" << boundingRect();

    QGraphicsTextItem::mouseMoveEvent(event);
}

void GraphicsTextItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    LOG(INFO) << "TextItem mouseReleaseEvent ";
    QGraphicsTextItem::mouseReleaseEvent(event);
}

void GraphicsTextItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *)
{
    setTextInteractionFlags(Qt::TextEditorInteraction);
}