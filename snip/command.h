#ifndef COMMAND_H
#define COMMAND_H

#include <QObject>
#include <QPen>
#include <vector>
#include <memory>
#include "textedit.h"

using std::shared_ptr;
using std::make_shared;
using std::vector;

class Command
{
public:
    enum CommandType{
        NONE,
        DRAW_ELLIPSE,
        DRAW_RECTANGLE,
        DRAW_ARROW,
        DRAW_BROKEN_LINE,
        DRAW_CURVE,
        DRAW_MOSAIC,
        DRAW_TEXT,
    };
public:
    Command(CommandType type) : type_(type) { }
    Command(CommandType type, const QPen& pen)
        : type_(type), pen_(pen)
    { }
    Command(CommandType type, const QPen& pen, bool is_fill, const std::vector<QPoint>& points)
        : type_(type), pen_(pen), is_fill_(is_fill), points_(points)
    { }

    inline void type(CommandType t) { type_ = t; }
    inline CommandType type() const  { return type_; }

    inline void pen(const QPen& pen) { pen_ = pen; }
    inline QPen pen() const { return pen_; }

    inline void setFill(bool fill) { is_fill_ = fill; }
    inline bool isFill() const { return is_fill_; }

    inline void points(const std::vector<QPoint>& ps) { points_ = ps; }
    inline const std::vector<QPoint>& points() const { return points_; }
    inline std::vector<QPoint>& points() { return points_; }

    inline void widget(QWidget * widget) { widget_ = widget; }
    inline QWidget * widget() const { return widget_; }

private:
    CommandType type_{ NONE };
    QPen pen_{};
    bool is_fill_ = false;
    std::vector<QPoint> points_;
    QWidget * widget_ = nullptr;
};

class CommandStack : public QObject
{
    Q_OBJECT

public:
    inline void push(shared_ptr<Command> command)
    {
        if(stack_.empty())
            emit empty(false);

        stack_.push_back(command);

        emit changed(stack_.size());
        emit pushed();
    }

    inline void pop()
    {
        stack_.pop_back();

        if(stack_.empty())
            emit empty(true);

        emit changed(stack_.size());
        emit poped();
    }


    inline size_t size() const { return stack_.size(); }

    inline shared_ptr<Command> back() const { return stack_.back(); }

    inline std::vector<shared_ptr<Command>> commands() const { return stack_; }

    inline bool empty() const { return stack_.empty(); }

signals:
    void changed(size_t);
    void pushed();
    void poped();

    void empty(bool);

public slots:
    inline void clear() { stack_.clear(); }

private:
    std::vector<shared_ptr<Command>> stack_;
};


#endif // COMMAND_H
