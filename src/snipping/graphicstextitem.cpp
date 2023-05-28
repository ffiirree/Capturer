#include "graphicstextitem.h"

#include "logging.h"
#include "resizer.h"

#include <numbers>
#include <QAbstractTextDocumentLayout>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QTextDocument>

GraphicsTextItem::GraphicsTextItem(const QString& text, const QPointF& vs)
    : QGraphicsTextItem(text)
{
    setFlag(QGraphicsItem::ItemIsFocusable);
    setFlag(QGraphicsItem::ItemIsMovable);
    setTextInteractionFlags(Qt::TextEditorInteraction);
    setAcceptHoverEvents(true);

    setPos(vs.x(), vs.y());
}

void GraphicsTextItem::focusInEvent(QFocusEvent *event)
{
    if (onfocus) onfocus();
    QGraphicsTextItem::focusInEvent(event);
}

void GraphicsTextItem::focusOutEvent(QFocusEvent *event)
{
    setTextInteractionFlags(Qt::NoTextInteraction);
    if (onblur) onblur();
    QGraphicsTextItem::focusOutEvent(event);
}

void GraphicsTextItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    if (onhovered) onhovered(Resizer::EDITAREA);
    QGraphicsTextItem::hoverEnterEvent(event);
}

void GraphicsTextItem::hoverMoveEvent(QGraphicsSceneHoverEvent * event)
{
    QGraphicsTextItem::hoverMoveEvent(event);
}

void GraphicsTextItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    if (onhovered) onhovered(Resizer::OUTSIDE);
    QGraphicsTextItem::hoverLeaveEvent(event);
}

void GraphicsTextItem::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete) {
        setVisible(false);
    }

    QGraphicsTextItem::keyPressEvent(event);
}

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