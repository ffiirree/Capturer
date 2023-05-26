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

    CommandStack(const CommandStack& r)
        : QObject()
    {
        stack_ = r.stack_;
    }

    CommandStack& operator=(const CommandStack& other)
    {
        stack_ = other.stack_;
        emit changed(stack_.size());
        emit emptied(stack_.empty());
        return *this;
    }

    inline void push(const std::shared_ptr<PaintCommand>& command)
    {
        if (stack_.empty()) emit emptied(false);

        stack_.push_back(command);

        emit changed(stack_.size());
        emit pushed();
        emit increased();
    }

    inline void pop()
    {
        stack_.pop_back();

        if (stack_.empty()) emit emptied(true);

        emit changed(stack_.size());
        emit poped();
        emit decreased();
    }

    inline void remove(const std::shared_ptr<PaintCommand>& cmd)
    {
        stack_.erase(std::remove(stack_.begin(), stack_.end(), cmd), stack_.end());

        if (stack_.empty()) emit emptied(true);

        emit changed(stack_.size());
        emit removed();
        emit decreased();
    }

    [[nodiscard]] inline size_t size() const { return stack_.size(); }

    [[nodiscard]] inline std::shared_ptr<PaintCommand> back() const { return stack_.back(); }

    [[nodiscard]] inline std::vector<std::shared_ptr<PaintCommand>> commands() const { return stack_; }

    [[nodiscard]] inline bool empty() const { return stack_.empty(); }

signals:
    void increased();
    void decreased();
    void changed(size_t);
    void pushed();
    void poped();
    void removed();

    void emptied(bool); // emit when stack changed to be empty or not.

public slots:
    inline void clear()
    {
        if (!stack_.empty()) {
            emit changed(0);
            emit emptied(true);
            emit decreased();
        }

        stack_.clear();
    }

private:
    std::vector<std::shared_ptr<PaintCommand>> stack_;
};

#endif //! CAPTURER_COMMAND_H