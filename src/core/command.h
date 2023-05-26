#ifndef CAPTURER_COMMAND_H
#define CAPTURER_COMMAND_H

#include <QGraphicsItem>
#include <QPoint>

struct Command
{
    virtual void redo() = 0;
    virtual void undo() = 0;
};

struct MoveCommand : public Command
{
    MoveCommand(QGraphicsItem *item, const QPointF& offset)
        : item_(item),
          offset_(offset)
    {}

    void redo()
    {
        if (item_) item_->moveBy(offset_.x(), offset_.y());
    }

    void undo()
    {
        if (item_) item_->moveBy(-offset_.x(), -offset_.y());
    }

private:
    QGraphicsItem *item_{};
    QPointF offset_{};
};

struct DeleteCommand : public Command
{
    DeleteCommand(QGraphicsItem *item)
        : item_(item)
    {}

    void redo()
    {
        if (item_) item_->setVisible(false);
    }

    void undo()
    {
        if (item_) item_->setVisible(true);
    }

private:
    QGraphicsItem *item_{};
};

struct CreateCommand : public Command
{
    CreateCommand(QGraphicsItem *item)
        : item_(item)
    {}

    void redo()
    {
        if (item_) item_->setVisible(true);
    }

    void undo()
    {
        if (item_) item_->setVisible(false);
    }

private:
    QGraphicsItem *item_{};
};

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