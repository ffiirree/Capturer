#include "command.h"

#include "graphicsitems.h"
#include "logging.h"

#include <QGraphicsScene>

////

CreatedCommand::CreatedCommand(QGraphicsScene *scene, QGraphicsItem *item)
    : scene_(scene),
      item_(item)
{}

CreatedCommand::~CreatedCommand()
{
    if (!item_->scene()) delete item_;
}

void CreatedCommand::redo()
{
    if (scene_ && item_) {
        scene_->addItem(item_);
        item_->setFocus();
    }
}

void CreatedCommand::undo()
{
    if (scene_ && item_) scene_->removeItem(item_);
}

////

MoveCommand::MoveCommand(QGraphicsItem *item, const QPointF& opos)
    : item_(item),
      opos_(opos)
{
    npos_ = item_->pos();
}

void MoveCommand::redo()
{
    if (item_) item_->setPos(npos_);
}

void MoveCommand::undo()
{
    if (item_) item_->setPos(opos_);
}

////

ResizeCommand::ResizeCommand(GraphicsItemWrapper *item, const ResizerF& osize, ResizerLocation location)
    : item_(item),
      location_(location),
      osize_(osize)
{
    nsize_ = item_->geometry();
}

void ResizeCommand::redo()
{
    if (item_) item_->resize(nsize_, location_);
}

void ResizeCommand::undo()
{
    if (item_) item_->resize(osize_, location_);
}

////

RotateCommand::RotateCommand(GraphicsItemWrapper *item, qreal oangle)
    : item_(item)
{
    oangle_ = oangle;
    nangle_ = item_->angle();
}

void RotateCommand::redo()
{
    if (item_) item_->rotate(nangle_);
}
void RotateCommand::undo()
{
    if (item_) item_->rotate(oangle_);
}

////

DeleteCommand::DeleteCommand(QGraphicsScene *scene)
    : scene_(scene)
{
    item_ = scene_->focusItem();
    if (!item_) {
        auto selected = scene_->selectedItems();
        if (!selected.isEmpty()) {
            item_ = selected.first();
            item_->setSelected(false);
        }
    }
}

void DeleteCommand::redo()
{
    if (scene_ && item_) scene_->removeItem(item_);
}

void DeleteCommand::undo()
{
    if (scene_ && item_) scene_->addItem(item_);
}

////

PenChangedCommand::PenChangedCommand(GraphicsItemWrapper *item, const QPen& open, const QPen& npen)
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

bool PenChangedCommand::mergeWith(const QUndoCommand *other)
{
    const PenChangedCommand *cmd = static_cast<const PenChangedCommand *>(other);
    if (cmd->item_ == nullptr) return false;

    npen_ = cmd->npen_;

    return true;
}

////

FillChangedCommand::FillChangedCommand(GraphicsItemWrapper *item, bool filled)
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

BrushChangedCommand::BrushChangedCommand(GraphicsItemWrapper *item, const QBrush& o, const QBrush& n)
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

FontChangedCommand::FontChangedCommand(GraphicsItemWrapper *item, const QFont& o, const QFont& n)
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
