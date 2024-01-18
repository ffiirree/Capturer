#ifndef CAPTURER_COMMAND_H
#define CAPTURER_COMMAND_H

#include "resizer.h"

#include <QFont>
#include <QPen>
#include <QPoint>
#include <QUndoCommand>

class QGraphicsScene;
class QGraphicsItem;
class GraphicsItemWrapper;

////

class CreatedCommand final : public QUndoCommand
{
public:
    CreatedCommand(QGraphicsScene *scene, QGraphicsItem *item);

    ~CreatedCommand() override;

    void redo() override;
    void undo() override;

private:
    QGraphicsScene *scene_{};
    QGraphicsItem *item_{};
};

////

class MoveCommand final : public QUndoCommand
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

class ResizeCommand final : public QUndoCommand
{
public:
    ResizeCommand(GraphicsItemWrapper *item, const ResizerF& osize, ResizerLocation location);

    void redo() override;
    void undo() override;

private:
    GraphicsItemWrapper *item_{};
    ResizerLocation location_{};
    ResizerF osize_{};
    ResizerF nsize_{};
};

////

class RotateCommand final : public QUndoCommand
{
public:
    RotateCommand(GraphicsItemWrapper *item, qreal oangle);

    void redo() override;
    void undo() override;

private:
    GraphicsItemWrapper *item_{};
    qreal oangle_{};
    qreal nangle_{};
};

////

class DeleteCommand final : public QUndoCommand
{
public:
    explicit DeleteCommand(QGraphicsScene *scene);

    void redo() override;
    void undo() override;

private:
    QGraphicsScene *scene_{};
    QGraphicsItem *item_{};
};

////

class PenChangedCommand final : public QUndoCommand
{
public:
    PenChangedCommand(GraphicsItemWrapper *item, QPen open, QPen npen);

    void redo() override;
    void undo() override;

    bool mergeWith(const QUndoCommand *) override;
    [[nodiscard]] int id() const override { return 1'234; }

private:
    GraphicsItemWrapper *item_{};
    QPen open_{ Qt::NoPen };
    QPen npen_{ Qt::NoPen };
};

////

class FillChangedCommand final : public QUndoCommand
{
public:
    FillChangedCommand(GraphicsItemWrapper *item, bool filled);

    void redo() override;
    void undo() override;

private:
    GraphicsItemWrapper *item_{};
    bool filled_{};
};

////
class FontChangedCommand final : public QUndoCommand
{
public:
    FontChangedCommand(GraphicsItemWrapper *item, const QFont& open, const QFont& npen);

    void redo() override;
    void undo() override;

private:
    GraphicsItemWrapper *item_{};
    QFont ofont_{};
    QFont nfont_{};
};

#endif //! CAPTURER_COMMAND_H