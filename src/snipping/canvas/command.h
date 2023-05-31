#ifndef CAPTURER_COMMAND_H
#define CAPTURER_COMMAND_H

#include <memory>
#include <QFont>
#include <QPen>
#include <QPoint>

class QGraphicsItem;
class GraphicsItemInterface;

////

struct Command
{
    virtual void redo() = 0;
    virtual void undo() = 0;
};

////

struct LambdaCommand : public Command
{
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

struct MoveCommand : public Command
{
    MoveCommand(QGraphicsItem *item, const QPointF& offset);

    void redo() override;
    void undo() override;

private:
    QGraphicsItem *item_{};
    QPointF offset_{};
};

////

struct DeletedCommand : public Command
{
    DeletedCommand(QGraphicsItem *item);

    void redo() override;
    void undo() override;

private:
    QGraphicsItem *item_{};
};

////

struct CreatedCommand : public Command
{
    CreatedCommand(QGraphicsItem *item);

    void redo() override;
    void undo() override;

private:
    QGraphicsItem *item_{};
};

////

struct PenChangedCommand : public Command
{
    PenChangedCommand(GraphicsItemInterface *item, const QPen& open, const QPen& npen);

    void redo() override;
    void undo() override;

private:
    GraphicsItemInterface *item_{};
    QPen open_{};
    QPen npen_{};
};

////

struct FillChangedCommand : public Command
{
    FillChangedCommand(GraphicsItemInterface *item, bool filled_);

    void redo() override;
    void undo() override;

private:
    GraphicsItemInterface *item_{};
    bool filled_{};
};

////

struct BrushChangedCommand : public Command
{
    BrushChangedCommand(GraphicsItemInterface *item, const QBrush& open, const QBrush& npen);

    void redo() override;
    void undo() override;

private:
    GraphicsItemInterface *item_{};
    QBrush obrush_{};
    QBrush nbrush_{};
};

////

struct FontChangedCommand : public Command
{
    FontChangedCommand(GraphicsItemInterface *item, const QFont& open, const QFont& npen);

    void redo() override;
    void undo() override;

private:
    GraphicsItemInterface *item_{};
    QFont ofont_{};
    QFont nfont_{};
};

////

struct MovedCommand : public Command
{
    MovedCommand(QGraphicsItem *item, const QPointF& offset);

    void redo() override;
    void undo() override;

private:
    QGraphicsItem *item_{};
    QPointF offset_{};
};


////

class CommandStack : public QObject
{
    Q_OBJECT

public:
    CommandStack() = default;

    inline void push(const std::shared_ptr<Command>& command)
    {
        if (undo_stack_.empty()) emit emptied(0, false);

        undo_stack_.push_back(command);

        emit changed(undo_stack_.size());
        emit pushed();

        // clear the redo commands
        if (!redo_stack_.empty()) {
            redo_stack_.clear();
            emit emptied(1, true);
        }
    }

signals:
    void changed(size_t);
    void pushed();

    void emptied(int, bool);

public slots:

    inline void undo()
    {
        if (undo_stack_.empty()) return;

        if (redo_stack_.empty()) emit emptied(1, false);

        undo_stack_.back()->undo();
        redo_stack_.emplace_back(undo_stack_.back());
        undo_stack_.pop_back();

        if (undo_stack_.empty()) emit emptied(0, true);
    }

    inline void redo()
    {
        if (redo_stack_.empty()) return;

        if (undo_stack_.empty()) emit emptied(0, false);

        redo_stack_.back()->redo();
        undo_stack_.emplace_back(redo_stack_.back());
        redo_stack_.pop_back();

        if (redo_stack_.empty()) emit emptied(1, true);
    }

    inline void clear()
    {
        undo_stack_.clear();
        redo_stack_.clear();

        emit emptied(0, true);
        emit emptied(1, true);
    }

private:
    std::vector<std::shared_ptr<Command>> redo_stack_;
    std::vector<std::shared_ptr<Command>> undo_stack_;
};

#endif //! CAPTURER_COMMAND_H