#ifndef CAPTURER_COMMAND_H
#define CAPTURER_COMMAND_H

#include "resizer.h"

#include <memory>
#include <QFont>
#include <QPen>
#include <QPoint>
#include <QUndoCommand>

class QGraphicsScene;
class QGraphicsItem;
class GraphicsItemInterface;

////

class LambdaCommand : public QUndoCommand
{
public:
    LambdaCommand(const std::function<void()>& r, const std::function<void()>& u)
        : onredo(r),
          onundo(u)
    {}

    void redo() override { onredo(); }

    void undo() override { onundo(); }

private:
    std::function<void()> onredo = []() {};
    std::function<void()> onundo = []() {};
};

////

class CreatedCommand : public QUndoCommand
{
public:
    CreatedCommand(QGraphicsScene *scene, QGraphicsItem *item);

    void redo() override;
    void undo() override;

private:
    QGraphicsScene *scene_{};
    QGraphicsItem *item_{};
};

////

class MoveCommand : public QUndoCommand
{
public:
    MoveCommand(QGraphicsItem *item, const QPointF& opos);

    void redo() override;
    void undo() override;

private:
    QGraphicsItem *item_{};
    QPointF opos_{};
    QPointF npos_{};
};

////

class ResizeCommand : public QUndoCommand
{
public:
    ResizeCommand(GraphicsItemInterface *item, const ResizerF& osize, ResizerLocation);

    void redo() override;
    void undo() override;

private:
    GraphicsItemInterface *item_{};
    ResizerLocation location_{};
    ResizerF osize_{};
    ResizerF nsize_{};
};

////

class RotateCommand : public QUndoCommand
{
public:
    RotateCommand(GraphicsItemInterface *item, qreal oangle);

    void redo() override;
    void undo() override;

private:
    GraphicsItemInterface *item_{};
    qreal oangle_{};
    qreal nangle_{};
};

////

class DeleteCommand : public QUndoCommand
{
public:
    DeleteCommand(QGraphicsScene *scene);

    void redo() override;
    void undo() override;

private:
    QGraphicsScene *scene_{};
    QGraphicsItem *item_{};
};

////

class PenChangedCommand : public QUndoCommand
{
public:
    PenChangedCommand(GraphicsItemInterface *item, const QPen& open, const QPen& npen);

    void redo() override;
    void undo() override;

    bool mergeWith(const QUndoCommand *) override;
    int id() const override { return 1'234; }

private:
    GraphicsItemInterface *item_{};
    QPen open_{ Qt::NoPen };
    QPen npen_{ Qt::NoPen };
};

////

class FillChangedCommand : public QUndoCommand
{
public:
    FillChangedCommand(GraphicsItemInterface *item, bool filled_);

    void redo() override;
    void undo() override;

private:
    GraphicsItemInterface *item_{};
    bool filled_{};
};

////

class BrushChangedCommand : public QUndoCommand
{
public:
    BrushChangedCommand(GraphicsItemInterface *item, const QBrush& open, const QBrush& npen);

    void redo() override;
    void undo() override;

private:
    GraphicsItemInterface *item_{};
    QBrush obrush_{};
    QBrush nbrush_{};
};

////
class FontChangedCommand : public QUndoCommand
{
public:
    FontChangedCommand(GraphicsItemInterface *item, const QFont& open, const QFont& npen);

    void redo() override;
    void undo() override;

private:
    GraphicsItemInterface *item_{};
    QFont ofont_{};
    QFont nfont_{};
};

#endif //! CAPTURER_COMMAND_H