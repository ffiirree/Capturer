#include "command.h"

#include "graphicsitems.h"
#include "logging.h"

#include <QGraphicsScene>
#include <utility>

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
    if (scene_ && item_ && !item_->scene()) {
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
        if (auto selected = scene_->selectedItems(); !selected.isEmpty()) {
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

PenChangedCommand::PenChangedCommand(GraphicsItemWrapper *item, QPen open, QPen npen)
    : item_(item),
      open_(std::move(open)),
      npen_(std::move(npen))
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
    const auto *cmd = dynamic_cast<const PenChangedCommand *>(other);
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
