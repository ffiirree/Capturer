#include "command.h"
#include <cmath>

PaintCommand::PaintCommand(Graph type, const QPen& pen, bool is_fill, const QPoint& start_point, QWidget * parent)
    : graph_(type), pen_(pen), is_fill_(is_fill)
{
    switch (graph_) {
    // 1. movable and resizable without a widget
    case RECTANGLE:
    case ELLIPSE:
    case LINE:
    case ARROW: resizer_ = Resizer(start_point, start_point); break;

    // 2. nomovable and noresizable
    case MOSAIC:
    case ERASER:
    case CURVES: push_point(start_point); break;

    // 3. movable with a widget
    case TEXT:
        resizer_ = Resizer(start_point, start_point);

        widget_ = new TextEdit(parent);
        connect(widget_, &TextEdit::textChanged, [this]() { modified(PaintType::REPAINT_ALL); });

        widget_->setTextColor(pen.color());
        widget_->setFocus();
        widget_->move(start_point);
        widget_->show();
        break;
    default: break;
    }
}

PaintCommand& PaintCommand::operator=(const PaintCommand& cmd)
{
    this->graph_ = cmd.graph_;
    this->pen_ = cmd.pen_;
    this->font_ = cmd.font_;
    this->is_fill_ = cmd.is_fill_;
    this->points_ = cmd.points_;
    this->points_buff_ = cmd.points_buff_;

    this->resizer_ = cmd.resizer_;

    if(cmd.widget_ != nullptr) {
        widget_ = new TextEdit(static_cast<QWidget *>(cmd.parent()));
        connect(widget_, &TextEdit::textChanged, [this]() { modified(PaintType::REPAINT_ALL); });

        widget_->setFont(this->font_);
        widget_->setTextColor(this->pen_.color());
        widget_->text(cmd.widget_->text());

        widget_->setFocus();
        widget_->move(cmd.widget()->pos());
        widget_->show();
    }

    this->visible_ = cmd.visible_;
    this->pre_ = cmd.pre_;
    this->adjusted_ = false;

    return *this;
}

bool PaintCommand::push_point(const QPoint& pos)
{
    switch (graph()) {
    case Graph::RECTANGLE:
    case Graph::ELLIPSE:
    case Graph::LINE:
        resizer_.point2() = pos;
        resizer_.x2(pos.x()); resizer_.y2(pos.y()); emit modified(PaintType::DRAW_MODIFING);
        return true;

    case Graph::ARROW:
        resizer_.x2(pos.x()); resizer_.y2(pos.y()); emit modified(PaintType::DRAW_MODIFING);
        updateArrowPoints(resizer_.point1(), resizer_.point2());
        return true;

    case Graph::MOSAIC:
    case Graph::ERASER:
    case Graph::CURVES:
        points_buff_.push_back(pos); emit modified(PaintType::DRAW_MODIFIED);
        return true;

    case Graph::TEXT:      return false;
    default:        return false;
    }
}

void PaintCommand::move(const QPoint& diff)
{
    switch (graph()) {
    case Graph::RECTANGLE:
    case Graph::ELLIPSE:
    case Graph::LINE:
        resizer_.move(diff.x(), diff.y());
        break;

    case Graph::ARROW:
        resizer_.move(diff.x(), diff.y());
        updateArrowPoints(resizer_.point1(), resizer_.point2());
        break;

    case Graph::TEXT:
        widget()->move(widget()->pos() + diff);
        break;

    default: break;
    }

    adjusted_ = true;
    emit modified(PaintType::DRAW_MODIFING);
}

bool PaintCommand::isValid()
{
    switch (graph_) {
    case RECTANGLE:
    case ELLIPSE:
    case LINE:      return size() != QSize(1, 1);
    case CURVES:
    case MOSAIC:
    case ERASER:    return true;
    case ARROW:     return size() != QSize(1, 1);
    case TEXT:      return !widget_->toPlainText().isEmpty();
    case BROKEN_LINE: return true;
    default: return true;
    }
}

void PaintCommand::resize(Resizer::PointPosition position, const QPoint& diff)
{
    switch (position) {
    case Resizer::X1Y1_ANCHOR:  resizer_.rx1() += diff.x(); resizer_.ry1() += diff.y(); break;
    case Resizer::X2Y1_ANCHOR:  resizer_.rx2() += diff.x(); resizer_.ry1() += diff.y(); break;
    case Resizer::X1Y2_ANCHOR:  resizer_.rx1() += diff.x(); resizer_.ry2() += diff.y(); break;
    case Resizer::X2Y2_ANCHOR:  resizer_.rx2() += diff.x(); resizer_.ry2() += diff.y(); break;
    case Resizer::X1_ANCHOR:    resizer_.rx1() += diff.x(); break;
    case Resizer::X2_ANCHOR:    resizer_.rx2() += diff.x(); break;
    case Resizer::Y1_ANCHOR:    resizer_.ry1() += diff.y(); break;
    case Resizer::Y2_ANCHOR:    resizer_.ry2() += diff.y(); break;
    default: break;
    }
    if(graph() == Graph::ARROW) updateArrowPoints(resizer_.point1(), resizer_.point2());

    adjusted_ = true;
    emit modified(PaintType::DRAW_MODIFING);
}

void PaintCommand::updateArrowPoints(QPoint begin, QPoint end)
{
    points_.resize(6);

    double par = 10.0;
    double par2 = 27.0;
    double slopy = atan2((end.y() - begin.y()), (end.x() - begin.x()));
    double alpha = 30.0 * 3.1415926 / 180.0;

    points_[1] = QPoint(end.x() - int(par * cos(alpha + slopy)) - int(9 * cos(slopy)), end.y() - int(par*sin(alpha + slopy)) - int(9 * sin(slopy)));
    points_[5] = QPoint(end.x() - int(par * cos(alpha - slopy)) - int(9 * cos(slopy)), end.y() + int(par*sin(alpha - slopy)) - int(9 * sin(slopy)));

    points_[2] = QPoint(end.x() - int(par2 * cos(alpha + slopy)), end.y() - int(par2*sin(alpha + slopy)));
    points_[4] = QPoint(end.x() - int(par2 * cos(alpha - slopy)), end.y() + int(par2*sin(alpha - slopy)));

    points_[0] = begin;
    points_[3] = end;
}

void PaintCommand::draw(QPainter *painter, bool modified)
{
    if(!visible()) return;

    painter->save();
    painter->setPen(pen());
    if(isFill()) painter->setBrush(pen().color());

    switch (graph()) {
    case Graph::RECTANGLE:
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->drawRect(rect());
        break;

    case Graph::ELLIPSE:
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->drawEllipse(rect());
        break;

    case Graph::LINE:
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->drawLine(resizer_.point1(), resizer_.point2());
        break;

    case Graph::ARROW:
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->drawPolygon(points_);
        break;

    case Graph::CURVES:
    case Graph::ERASER:
    case Graph::MOSAIC:
        painter->setRenderHint(QPainter::Antialiasing, true);
        if(modified) {
            painter->drawPolyline(points_buff_);
            points_.append(points_buff_);
            points_buff_.clear();
            points_buff_ << points_.last();
        }
        // repaint
        else {
            (points().size() == 1) ? painter->drawPoint(points()[0]) : painter->drawPolyline(points_);
        }
        break;

    case Graph::TEXT:
        painter->setFont(widget_->font());
        painter->drawText(widget_->geometry().adjusted(2, 2, 0, 0), Qt::TextWordWrap, widget_->toPlainText());
        break;

    default: break;
    }
    painter->restore();
}


void PaintCommand::draw_modified(QPainter *painter)
{
    draw(painter, true);
}

void PaintCommand::repaint(QPainter *painter)
{
    draw(painter, false);
}
