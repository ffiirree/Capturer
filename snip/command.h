#ifndef COMMAND_H
#define COMMAND_H

#include <QObject>
#include <QPen>
#include <vector>
#include <memory>
#include "utils.h"
#include "textedit.h"

using std::shared_ptr;
using std::make_shared;
using std::vector;

class PaintCommand
{
public:
    PaintCommand(Graph type) : graph_(type) { }
    PaintCommand(Graph type, const QPen& pen)
        : graph_(type), pen_(pen)
    { }
    PaintCommand(Graph type, const QPen& pen, bool is_fill, const QVector<QPoint>& points)
        : graph_(type), pen_(pen), is_fill_(is_fill), points_(points)
    { }

    inline void graph(Graph t) { graph_ = t; }
    inline Graph graph() const  { return graph_; }

    inline void pen(const QPen& pen) { pen_ = pen; }
    inline QPen pen() const { return pen_; }

    inline void setFill(bool fill) { is_fill_ = fill; }
    inline bool isFill() const { return is_fill_; }

    inline void points(const QVector<QPoint>& ps) { points_ = ps; }
    inline const QVector<QPoint>& points() const { return points_; }
    inline QVector<QPoint>& points() { return points_; }

    inline void widget(QWidget * widget) { widget_ = widget; }
    inline QWidget * widget() const { return widget_; }

    void execute(QPainter *);

    QRect rect();
    QSize size();

private:
    void getArrowPoints(QPoint, QPoint, QPoint*);

    Graph graph_{ NONE };
    QPen pen_{};
    bool is_fill_ = false;
    QVector<QPoint> points_;
    QWidget * widget_ = nullptr;

    QRect rect_;
};

class CommandStack : public QObject
{
    Q_OBJECT

public:
    CommandStack() = default;

    CommandStack(const CommandStack& other)
    {
        stack_ = other.stack_;
    }
    CommandStack& operator=(const CommandStack& other){
        stack_ = other.stack_;
        emit changed(stack_.size());
        emit emptied(!stack_.size());
        return *this;
    }

    inline void push(shared_ptr<PaintCommand> command)
    {
        if(stack_.empty())
            emit emptied(false);

        stack_.push_back(command);

        emit changed(stack_.size());
        emit pushed();
    }

    inline void pop()
    {
        stack_.pop_back();

        if(stack_.empty())
            emit emptied(true);

        emit changed(stack_.size());
        emit poped();
    }

    inline size_t size() const { return stack_.size(); }

    inline shared_ptr<PaintCommand> back() const { return stack_.back(); }

    inline vector<shared_ptr<PaintCommand>> commands() const { return stack_; }

    inline bool empty() const { return stack_.empty(); }

signals:
    void changed(size_t);
    void pushed();
    void poped();

    void emptied(bool);               // emit when stack changed to be empty or not.

public slots:
    inline void clear()
    {
        if(!stack_.empty()) {
            emit changed(0);
            emit emptied(true);
        }

        stack_.clear();
    }

private:
    vector<shared_ptr<PaintCommand>> stack_;
};


#endif // COMMAND_H
