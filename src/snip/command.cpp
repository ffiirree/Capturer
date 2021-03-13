#include "command.h"
#include <cmath>

PaintCommand::PaintCommand(Graph type, const QPen& pen, const QFont& font, bool is_fill, const QPoint& start_point)
    : graph_(type), pen_(pen), font_(font), is_fill_(is_fill)
{
    switch (graph_) {
    // 1. movable and resizable without a widget
    case Graph::RECTANGLE:
    case Graph::ELLIPSE:
    case Graph::LINE:
    case Graph::ARROW: resizer_ = Resizer(start_point, start_point); break;

    // 2. nomovable and noresizable
    case Graph::MOSAIC:
    case Graph::ERASER:
    case Graph::CURVES: push_point(start_point); break;

    // 3. movable with a widget
    case Graph::TEXT:
        resizer_ = Resizer(start_point, start_point);

        widget_ = make_shared<TextEdit>();
        connect(widget_.get(), &TextEdit::textChanged, [this]() { modified(PaintType::REPAINT_ALL); });

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
        widget_ = make_shared<TextEdit>();
        connect(widget_.get(), &TextEdit::textChanged, [this]() { modified(PaintType::REPAINT_ALL); });

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
    switch (graph_) {
    case Graph::RECTANGLE:
    case Graph::ELLIPSE:
    case Graph::LINE:
        resizer_.x2(pos.x()); resizer_.y2(pos.y()); emit modified(PaintType::DRAW_MODIFYING);
        return true;

    case Graph::ARROW:
        resizer_.x2(pos.x()); resizer_.y2(pos.y()); emit modified(PaintType::DRAW_MODIFYING);
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
    emit modified(PaintType::DRAW_MODIFYING);
}

bool PaintCommand::isValid()
{
    switch (graph_) {
    case Graph::RECTANGLE:
    case Graph::ELLIPSE:
    case Graph::LINE:      return size() != QSize(1, 1);
    case Graph::CURVES:
    case Graph::MOSAIC:
    case Graph::ERASER:    return true;
    case Graph::ARROW:     return size() != QSize(1, 1);
    case Graph::TEXT:      return !widget_->toPlainText().isEmpty();
    case Graph::BROKEN_LINE: return true;
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
    emit modified(PaintType::DRAW_MODIFYING);
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

void PaintCommand::drawAnchors(QPainter* painter)
{
    painter->save();

    switch (graph()) {
    case Graph::RECTANGLE:
    case Graph::ELLIPSE:
        painter->setRenderHint(QPainter::Antialiasing, false);
        painter->setBrush(Qt::NoBrush);

        // box border
        painter->setPen(QPen(Qt::black, 1, Qt::DashLine));
        painter->drawRect(resizer().rect());

        // anchors
        painter->setPen(QPen(Qt::black, 1, Qt::SolidLine));
        painter->drawRects(resizer().anchors());

        break;

    case Graph::LINE:
    case Graph::ARROW:
        painter->setPen(QPen(Qt::black, 1, Qt::DashLine));
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->drawLine(resizer().point1(), resizer().point2());

        painter->setPen(QPen(Qt::black, 1, Qt::SolidLine));
        painter->drawRect(resizer().X1Y1Anchor());
        painter->drawRect(resizer().X2Y2Anchor());
        break;

    case Graph::TEXT:
    {
        Resizer resizer(geometry().adjusted(-5, -5, 5, 5));

        painter->save();
        painter->setPen(QPen(QColor("#333"), 2, Qt::SolidLine));
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setBrush(Qt::white);
        painter->drawEllipse(resizer.rotateAnchor());
        painter->restore();

        // box border
        painter->setPen(QPen(Qt::black, 1, Qt::DashLine));
        painter->drawRect(resizer.rect());

        // anchors
        painter->setPen(QPen(Qt::black, 1, Qt::SolidLine));
        painter->drawRects(resizer.cornerAnchors());
        break;
    }

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

bool PaintCommand::regularized()
{
    switch (graph_)
    {
    case Graph::RECTANGLE:
    case Graph::ELLIPSE:   return resizer_.width() == resizer_.height();
    case Graph::LINE:
    case Graph::ARROW:     return (resizer_.x1() == resizer_.x2()) || (resizer_.y1() == resizer_.y2());
    default:        return true;
    }
}

void PaintCommand::regularize()
{
    switch (graph())
    {
    case Graph::CIRCLE:
    case Graph::RECTANGLE:
    {
        int width = std::sqrt(resizer_.width() * resizer_.height());
        QRect rect{ QPoint{ 0, 0 }, QPoint{ width, width } };
        rect.moveCenter(resizer_.rect().center());

        resizer_.rx1() = rect.topLeft().x();
        resizer_.ry1() = rect.topLeft().y();

        push_point(rect.bottomRight());

        break;
    }
    case Graph::LINE:
    case Graph::ARROW:
    {
        auto p1 = resizer_.point1();
        auto p2 = resizer_.point2();

        auto theta = std::atan((double)std::abs(p1.x() - p2.x()) / std::abs(p1.y() - p2.y())) * 180.0 / 3.1415926;

        if (theta > 80) {
            p2.setY(p1.y());
        }
        else if (theta < 20) {
            p2.setX(p1.x());
        }

        resizer_.rx1() = p1.x();
        resizer_.ry1() = p1.y();

        push_point(p2);

        break;
    }
    default: break;
    }
}

Resizer::PointPosition PaintCommand::hover(const QPoint& pos)
{
    switch (graph_)
    {
    case RECTANGLE: return resizer().absolutePos(pos);
    case CIRCLE:
    {
        QRegion r1(rect().adjusted(-2, -2, 2, 2), QRegion::Ellipse);
        QRegion r2(rect().adjusted(2, 2, -2, -2), QRegion::Ellipse);

        auto hover_pos = resizer_.absolutePos(pos);
        if (!(hover_pos & Resizer::ADJUST_AREA) && r1.contains(pos) && !r2.contains(pos)) {
            return Resizer::BORDER;
        }

        return hover_pos;
    }
    case LINE:
    case ARROW:
    {
        auto x1y1_anchor = resizer().X1Y1Anchor();
        auto x2y2_anchor = resizer().X2Y2Anchor();
        QPolygon polygon;
        polygon.push_back(x1y1_anchor.topLeft());
        polygon.push_back(x1y1_anchor.bottomRight());
        polygon.push_back(x2y2_anchor.bottomRight());
        polygon.push_back(x2y2_anchor.topLeft());

        if (x1y1_anchor.contains(pos)) {
            return Resizer::X1Y1_ANCHOR;
        }
        else if (x2y2_anchor.contains(pos)) {
            return Resizer::X2Y2_ANCHOR;
        }
        else if (polygon.containsPoint(pos, Qt::OddEvenFill)) {
            return Resizer::BORDER;
        }
    }
    case TEXT:
    {
        Resizer resizer(geometry().adjusted(-5, -5, 5, 5));
        resizer.enableRotate(true);
        auto hover_pos = resizer.absolutePos(pos);
        switch (hover_pos) {
        case Resizer::INSIDE:
        case Resizer::X1_ANCHOR:
        case Resizer::Y2_ANCHOR:
        case Resizer::Y1_ANCHOR:
        case Resizer::X2_ANCHOR: hover_pos = Resizer::BORDER; break;
        default: break;
        }
        return hover_pos;
    }
    default:
        break;
    }
}

QCursor PaintCommand::getCursorShapeByHoverPos(Resizer::PointPosition pos, const QCursor& default_cursor)
{
    switch (pos) {
    case Resizer::X1_ANCHOR:
    case Resizer::X2_ANCHOR:    return Qt::SizeHorCursor;
    case Resizer::Y1_ANCHOR:
    case Resizer::Y2_ANCHOR:    return Qt::SizeVerCursor;
    case Resizer::X1Y1_ANCHOR:
    case Resizer::X2Y2_ANCHOR:  return Qt::SizeFDiagCursor;
    case Resizer::X1Y2_ANCHOR:
    case Resizer::X2Y1_ANCHOR:  return Qt::SizeBDiagCursor;

    case Resizer::BORDER:
    case Resizer::X1_BORDER:
    case Resizer::X2_BORDER:
    case Resizer::Y1_BORDER:
    case Resizer::Y2_BORDER:    return Qt::SizeAllCursor;
    case Resizer::ROTATE_ANCHOR: return (QPixmap(":/icon/res/rotate").scaled(22, 22, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));

    default: return default_cursor;
    }
}