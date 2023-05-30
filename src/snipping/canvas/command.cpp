#include "command.h"

#include "graphicsitems.h"
#include "logging.h"

#include <QGraphicsItem>

////

MoveCommand::MoveCommand(QGraphicsItem *item, const QPointF& offset)
    : item_(item),
      offset_(offset)
{}

void MoveCommand::redo()
{
    if (item_) {
        item_->moveBy(offset_.x(), offset_.y());
    }
}

void MoveCommand::undo()
{
    if (item_) {
        item_->moveBy(-offset_.x(), -offset_.y());
    }
}

////

DeletedCommand::DeletedCommand(QGraphicsItem *item)
    : item_(item)
{}

void DeletedCommand::redo()
{
    if (item_) item_->setVisible(false);
}

void DeletedCommand::undo()
{
    if (item_) item_->setVisible(true);
}

////

CreatedCommand::CreatedCommand(QGraphicsItem *item)
    : item_(item)
{}

void CreatedCommand::redo()
{
    if (item_) item_->setVisible(true);
}

void CreatedCommand::undo()
{
    if (item_) item_->setVisible(false);
}

////

PenChangedCommand::PenChangedCommand(GraphicsItemInterface *item, const QPen& open, const QPen& npen)
    : item_(item),
      open_(open),
      npen_(npen)
{}

void PenChangedCommand::redo()
{
    if (item_) item_->setPen(npen_);
}

void PenChangedCommand::undo()
{
    if (item_) item_->setPen(open_);
}

////

FillChangedCommand::FillChangedCommand(GraphicsItemInterface *item, bool filled)
    : item_(item),
      filled_(filled)
{}

void FillChangedCommand::redo()
{
    if (item_) item_->fill(filled_);
}

void FillChangedCommand::undo()
{
    if (item_) item_->fill(!filled_);
}

////

BrushChangedCommand::BrushChangedCommand(GraphicsItemInterface *item, const QBrush& o, const QBrush& n)
    : item_(item),
      obrush_(o),
      nbrush_(n)
{}

void BrushChangedCommand::redo()
{
    if (item_) item_->setBrush(nbrush_);
}

void BrushChangedCommand::undo()
{
    if (item_) item_->setBrush(obrush_);
}

////

FontChangedCommand::FontChangedCommand(GraphicsItemInterface *item, const QFont& o, const QFont& n)
    : item_(item),
      ofont_(o),
      nfont_(n)
{}

void FontChangedCommand::redo()
{
    if (item_) item_->setFont(nfont_);
}

void FontChangedCommand::undo()
{
    if (item_) item_->setFont(ofont_);
}

////

MovedCommand::MovedCommand(QGraphicsItem *item, const QPointF& offset)
    : item_(item),
      offset_(offset)
{}

void MovedCommand::redo()
{
    if (item_) item_->moveBy(offset_.x(), offset_.y());
}

void MovedCommand::undo()
{
    if (item_) item_->moveBy(-offset_.x(), -offset_.y());
}

////

ResizedCommand::ResizedCommand(QGraphicsItem *item, const QMarginsF& margins)
    : item_(item),
      margins_(margins)
{}

void ResizedCommand::redo()
{
    // if (item_) item_->
}

void ResizedCommand::undo()
{
    // if (item_) item_->moveBy(-offset_.x(), -offset_.y());
}
