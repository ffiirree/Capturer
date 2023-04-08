#ifndef COMMAND_H
#define COMMAND_H

#include <QPen>
#include <utility>
#include "utils.h"
#include "resizer.h"
#include "textedit.h"
#include "logging.h"

class PaintCommand : public QObject
{
    Q_OBJECT
public:
    PaintCommand(Graph, const QPen&, const QFont&, bool, const QPoint&, const QPoint&);

    PaintCommand(const PaintCommand& cmd) : QObject() { *this = cmd; }
    PaintCommand& operator=(const PaintCommand&);

    [[nodiscard]] inline Graph graph() const  { return graph_; }

    inline void pen(const QPen& pen) 
    {
        if (graph_ == Graph::ERASER || graph_ == Graph::MOSAIC) return;
        pen_ = pen; emit modified(PaintType::REPAINT_ALL); 
    }
    [[nodiscard]] inline QPen pen() const { return pen_; }

    void font(const QFont& font);
    [[nodiscard]] inline QFont font() const { return font_; }

    inline void setFill(bool fill) { 
        if (graph_ == Graph::RECTANGLE || graph_ == Graph::ELLIPSE) {
            is_fill_ = fill; 
            emit modified(PaintType::REPAINT_ALL);
        }
    }
    [[nodiscard]] inline bool isFill() const { return is_fill_; }

    [[nodiscard]] inline QVector<QPoint> points() const { return points_; }
    inline QVector<QPoint> points() { return points_; }

    inline QRect geometry() { if(widget_) return widget_->geometry(); return {}; }

    bool push_point(const QPoint& pos);
    void move(const QPoint& diff);
    void resize(Resizer::PointPosition, const QPoint&);
    void rotate(const QPoint&);

    inline void setFocus(bool f)
    {
        if(widget_) {
            f ? widget_->show(), widget_->activateWindow() : widget_->hide();
        }

        emit modified(PaintType::UPDATE_MASK);
    }

    bool isValid();

    void drawAnchors(QPainter*);
    void draw_modified(QPainter *);
    void repaint(QPainter *);

    void regularize();
    bool regularized();

    Resizer::PointPosition hover(const QPoint&);
    static QCursor getCursorShapeByHoverPos(Resizer::PointPosition, const QCursor & = Qt::CrossCursor);

    QRect rect() { return resizer_.rect(); }
    QSize size() { return rect().size(); }

    [[nodiscard]] Resizer resizer() const { return resizer_; }

    void visible(bool v)
    {
        if(visible_ != v) {
            emit modified(v ? PaintType::DRAW_FINISHED : PaintType::REPAINT_ALL);
            visible_ = v;
        }
    }
    [[nodiscard]] bool visible() const { return visible_; }

    void previous(std::shared_ptr<PaintCommand> pre) { pre_ = std::move(pre); }
    [[nodiscard]] std::shared_ptr<PaintCommand> previous() const { return pre_; }

    [[nodiscard]] bool adjusted() const { return adjusted_; }
signals:
    void modified(PaintType);
    void styleChanged();

private:
    void updateArrowPoints(QPoint, QPoint);
    void draw(QPainter *, bool modified);

    Graph graph_{ Graph::NONE };
    QPen pen_;
    QFont font_;
    float theta_{ 0 };
    bool is_fill_{ false };
    QVector<QPoint> points_;
    QVector<QPoint> points_buff_;
    std::shared_ptr<TextEdit> widget_{ nullptr };

    Resizer resizer_{};

    bool visible_{ true };
    std::shared_ptr<PaintCommand> pre_{ nullptr };

    bool adjusted_ = false;
    QPoint offset_{ 0,0 };
};

class CommandStack : public QObject
{
    Q_OBJECT

public:
    CommandStack() = default;

    CommandStack(const CommandStack& r) :QObject() { stack_ = r.stack_; }

    CommandStack& operator=(const CommandStack& other)
    {
        stack_ = other.stack_;
        emit changed(stack_.size());
        emit emptied(stack_.empty());
        return *this;
    }

    inline void push(const std::shared_ptr<PaintCommand>& command)
    {
        if(stack_.empty())
            emit emptied(false);

        stack_.push_back(command);

        emit changed(stack_.size());
        emit pushed();
        emit increased();
    }

    inline void pop()
    {
        stack_.pop_back();

        if(stack_.empty())
            emit emptied(true);

        emit changed(stack_.size());
        emit poped();
        emit decreased();
    }

    inline void remove(const std::shared_ptr<PaintCommand>& cmd)
    {
        stack_.erase(std::remove(stack_.begin(), stack_.end(), cmd), stack_.end());

        if(stack_.empty())
            emit emptied(true);

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

    void emptied(bool);               // emit when stack changed to be empty or not.

public slots:
    inline void clear()
    {
        if(!stack_.empty()) {
            emit changed(0);
            emit emptied(true);
            emit decreased();
        }

        stack_.clear();
    }

private:
    std::vector<std::shared_ptr<PaintCommand>> stack_;
};


#endif // COMMAND_H
