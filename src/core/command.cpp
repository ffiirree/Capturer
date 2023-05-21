#include "command.h"

#include <cmath>
#include <numbers>
#include <QPainter>

#define TEXT_EDIT_MARGIN 16

PaintCommand::PaintCommand(Graph type, const QPen& pen, const QFont& font, bool is_fill,
                           const QPoint& start_point, const QPoint& offset)
    : QObject(), graph_(type), pen_(pen), font_(font), is_fill_(is_fill), offset_(offset)
{
    switch (graph_) {
    // 1. movable and resizable without a widget
    case Graph::RECTANGLE:
    case Graph::ELLIPSE:
    case Graph::LINE:
    case Graph::ARROW: resizer_ = Resizer(start_point, start_point); break;

    // 2. unmovable and unresizable
    case Graph::MOSAIC:
    case Graph::ERASER:
    case Graph::CURVES: push_point(start_point); break;

    // 3. movable with a widget
    case Graph::TEXT:
        widget_ = std::make_shared<TextEdit>();
        connect(widget_.get(), &TextEdit::textChanged, [this]() {
            resizer_.resize(widget_->size() + QSize{ TEXT_EDIT_MARGIN, TEXT_EDIT_MARGIN }); // padding 5
            modified(PaintType::REPAINT_ALL);
        });

        widget_->setFont(font_);
        widget_->setColor(pen_.color());

        resizer_ = Resizer(start_point, widget_->size() + QSize{ TEXT_EDIT_MARGIN, TEXT_EDIT_MARGIN });

        widget_->move(resizer_.topLeft() + QPoint{ TEXT_EDIT_MARGIN / 2, TEXT_EDIT_MARGIN / 2 } + offset_);
        widget_->show();
        widget_->activateWindow();
        break;
    default: break;
    }
}

PaintCommand& PaintCommand::operator=(const PaintCommand& other)
{
    graph_       = other.graph_;
    pen_         = other.pen_;
    font_        = other.font_;
    is_fill_     = other.is_fill_;
    theta_       = other.theta_;
    points_      = other.points_;
    points_buff_ = other.points_buff_;

    resizer_ = other.resizer_;

    if (other.widget_ != nullptr) {
        widget_ = std::make_shared<TextEdit>();

        widget_->setFont(other.widget_->font());
        widget_->setColor(pen_.color());

        widget_->setText(other.widget_->toPlainText());

        connect(widget_.get(), &TextEdit::textChanged, [this]() {
            resizer_.resize(widget_->size() + QSize{ TEXT_EDIT_MARGIN, TEXT_EDIT_MARGIN });
            modified(PaintType::REPAINT_ALL);
        });

        widget_->move(resizer_.topLeft() + QPoint{ TEXT_EDIT_MARGIN / 2, TEXT_EDIT_MARGIN / 2 } +
                      other.offset_);
        widget_->show();
        widget_->activateWindow();
    }

    visible_  = other.visible_;
    pre_      = other.pre_;
    adjusted_ = false;
    offset_   = other.offset_;

    return *this;
}

void PaintCommand::font(const QFont& font)
{
    if (graph_ != Graph::TEXT) return;

    font_ = font;
    widget_->setFont(font);
    resizer_.resize(widget_->size() + QSize{ TEXT_EDIT_MARGIN, TEXT_EDIT_MARGIN });
    emit modified(PaintType::REPAINT_ALL);
}

bool PaintCommand::push_point(const QPoint& pos)
{
    switch (graph_) {
    case Graph::RECTANGLE:
    case Graph::ELLIPSE:
    case Graph::LINE:
        resizer_.x2(pos.x());
        resizer_.y2(pos.y());
        emit modified(PaintType::DRAW_MODIFYING);
        return true;

    case Graph::ARROW:
        resizer_.x2(pos.x());
        resizer_.y2(pos.y());
        emit modified(PaintType::DRAW_MODIFYING);
        updateArrowPoints(resizer_.point1(), resizer_.point2());
        return true;

    case Graph::MOSAIC:
    case Graph::ERASER:
    case Graph::CURVES:
        points_buff_.push_back(pos);
        emit modified(PaintType::DRAW_MODIFIED);
        return true;

    case Graph::TEXT:
    default: return false;
    }
}

void PaintCommand::move(const QPoint& diff)
{
    switch (graph()) {
    case Graph::RECTANGLE:
    case Graph::ELLIPSE:
    case Graph::LINE: resizer_.translate(diff.x(), diff.y()); break;

    case Graph::ARROW:
        resizer_.translate(diff.x(), diff.y());
        updateArrowPoints(resizer_.point1(), resizer_.point2());
        break;

    case Graph::TEXT:
        resizer_.translate(diff.x(), diff.y());
        widget_->move(resizer_.topLeft() + QPoint{ TEXT_EDIT_MARGIN / 2, TEXT_EDIT_MARGIN / 2 } + offset_);
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
    case Graph::LINE: return size() != QSize(1, 1);
    case Graph::CURVES:
    case Graph::MOSAIC:
    case Graph::ERASER: return true;
    case Graph::ARROW: return size() != QSize(1, 1);
    case Graph::TEXT: return !widget_->toPlainText().isEmpty();
    case Graph::BROKEN_LINE:
    default: return true;
    }
}

void PaintCommand::rotate(const QPoint& point)
{
    QPoint center = rect().center();
    theta_        = atan2(point.x() - center.x(), center.y() - point.y()) * 180.0 / 3.1415926;

    emit modified(PaintType::DRAW_MODIFYING);
}

void PaintCommand::resize(Resizer::PointPosition position, const QPoint& cursor_pos)
{
    auto pre_resizer = resizer_;
    if (graph() == TEXT && widget_->toPlainText().isEmpty()) return;

    // clang-format off
    switch (position) {
    case Resizer::X1Y1_ANCHOR:  resizer_.x1(cursor_pos.x()); resizer_.y1(cursor_pos.y()); break;
    case Resizer::X2Y1_ANCHOR:  resizer_.x2(cursor_pos.x()); resizer_.y1(cursor_pos.y()); break;
    case Resizer::X1Y2_ANCHOR:  resizer_.x1(cursor_pos.x()); resizer_.y2(cursor_pos.y()); break;
    case Resizer::X2Y2_ANCHOR:  resizer_.x2(cursor_pos.x()); resizer_.y2(cursor_pos.y()); break;
    case Resizer::X1_ANCHOR:    resizer_.x1(cursor_pos.x()); break;
    case Resizer::X2_ANCHOR:    resizer_.x2(cursor_pos.x()); break;
    case Resizer::Y1_ANCHOR:    resizer_.y1(cursor_pos.y()); break;
    case Resizer::Y2_ANCHOR:    resizer_.y2(cursor_pos.y()); break;
    default: break;
    }
    // clang-format on

    switch (graph()) {
    case Graph::ARROW:
        updateArrowPoints(resizer_.point1(), resizer_.point2());
        break;
        // keep aspect ratio
    case Graph::TEXT: {
        QSize text_size = widget_->size();
        QSize diff;

        if (pre_resizer.rect().contains(resizer_.rect())) {
            auto r = resizer_.rect().intersected(pre_resizer.rect());
            diff =
                text_size.scaled(r.size() - QSize(TEXT_EDIT_MARGIN, TEXT_EDIT_MARGIN),
                                 position & 0x0f00 ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio) -
                (resizer_.size() - QSize{ TEXT_EDIT_MARGIN, TEXT_EDIT_MARGIN });
        }
        else {
            auto r = resizer_.rect().united(pre_resizer.rect());
            diff   = text_size.scaled(r.size() - QSize(TEXT_EDIT_MARGIN, TEXT_EDIT_MARGIN),
                                      Qt::KeepAspectRatioByExpanding) -
                   (resizer_.size() - QSize{ TEXT_EDIT_MARGIN, TEXT_EDIT_MARGIN });
        }

        // clang-format off
        switch (position) {
        case Resizer::X1Y1_ANCHOR:  resizer_.adjust(-diff.width(),  -diff.height(), 0,              0            ); break;
        case Resizer::X2Y1_ANCHOR:  resizer_.adjust(0,              -diff.height(), diff.width(),   0            ); break;
        case Resizer::X1Y2_ANCHOR:  resizer_.adjust(-diff.width(),  0,              0,              diff.height()); break;
        case Resizer::X2Y2_ANCHOR:  resizer_.adjust(0,              0,              diff.width(),   diff.height()); break;
        case Resizer::X1_ANCHOR:    resizer_.adjust(0,              0,              0,              diff.height()); break;
        case Resizer::X2_ANCHOR:    resizer_.adjust(0,              0,              0,              diff.height()); break;
        case Resizer::Y1_ANCHOR:    resizer_.adjust(0,              0,              diff.width(),   0            ); break;
        case Resizer::Y2_ANCHOR:    resizer_.adjust(0,              0,              diff.width(),   0            ); break;
        default: break;
        }
        // clang-format on

        float font_size = std::max<float>(1, font_.pointSizeF() * ((rect().width() - TEXT_EDIT_MARGIN) /
                                                                   std::max<float>(text_size.width(), 10)));
        font_.setPointSizeF(std::round(font_size * 100.0) / 100.0);
        widget_->setFont(font_);

        emit styleChanged();

        widget_->move(resizer_.topLeft() + QPoint{ TEXT_EDIT_MARGIN / 2, TEXT_EDIT_MARGIN / 2 } + offset_);
        break;
    }
    default: break;
    }

    adjusted_ = true;
    emit modified(PaintType::DRAW_MODIFYING);
}

void PaintCommand::updateArrowPoints(QPoint begin, QPoint end)
{
    points_.resize(6);

    double slopy = std::atan2(end.y() - begin.y(), end.x() - begin.x());

    double par1 = 20.0;
    double par2 = 25.0;

    double alpha1 = 13.0 * std::numbers::pi / 180.0;
    double alpha2 = 26.0 * std::numbers::pi / 180.0;

    points_[0] = begin;

    points_[1] = { end.x() - par1 * std::cos(alpha1 + slopy), end.y() - par1 * std::sin(alpha1 + slopy) };
    points_[5] = { end.x() - par1 * std::cos(alpha1 - slopy), end.y() + par1 * std::sin(alpha1 - slopy) };

    points_[2] = { end.x() - par2 * std::cos(alpha2 + slopy), end.y() - par2 * std::sin(alpha2 + slopy) };
    points_[4] = { end.x() - par2 * std::cos(alpha2 - slopy), end.y() + par2 * std::sin(alpha2 - slopy) };

    points_[3] = end;
}

void PaintCommand::draw(QPainter *painter, bool modified)
{
    if (!visible()) return;

    painter->save();

    pen_.setCapStyle(Qt::RoundCap);
    pen_.setJoinStyle(Qt::RoundJoin);
    painter->setPen(pen_);
    if (isFill()) painter->setBrush(pen().color());

    painter->setRenderHint(QPainter::Antialiasing, true);

    switch (graph()) {
    case Graph::RECTANGLE: painter->drawRect(rect()); break;

    case Graph::ELLIPSE: painter->drawEllipse(rect()); break;

    case Graph::LINE: painter->drawLine(resizer_.point1(), resizer_.point2()); break;

    case Graph::ARROW:
        painter->setPen(QPen(pen_.color(), 1, Qt::SolidLine, Qt::SquareCap, Qt::BevelJoin));
        painter->drawPolygon(points_);
        break;

    case Graph::CURVES:
    case Graph::ERASER:
    case Graph::MOSAIC:
        if (modified) {
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
        painter->drawText(rect().adjusted(TEXT_EDIT_MARGIN / 2, TEXT_EDIT_MARGIN / 2, -TEXT_EDIT_MARGIN / 2,
                                          -TEXT_EDIT_MARGIN / 2),
                          Qt::AlignVCenter, widget_->toPlainText());
        break;

    default: break;
    }
    painter->restore();
}

void PaintCommand::drawAnchors(QPainter *painter)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    auto color = QColor("#969696");

    switch (graph()) {
    case Graph::RECTANGLE:
    case Graph::ELLIPSE:
        painter->setBrush(Qt::NoBrush);

        // box border
        painter->setPen(QPen(color, 1, Qt::DashLine));
        painter->drawRect(rect());

        // anchors
        painter->setBrush(Qt::white);
        painter->setPen(QPen(color, 1, Qt::SolidLine));
        painter->drawRects(resizer().anchors());
        break;

    case Graph::LINE:
    case Graph::ARROW:
        painter->setPen(QPen(color, 1, Qt::DashLine));
        painter->drawLine(resizer().point1(), resizer().point2());

        painter->setPen(QPen(color, 1, Qt::SolidLine));
        painter->setBrush(Qt::white);
        painter->drawRect(resizer().X1Y1Anchor());
        painter->drawRect(resizer().X2Y2Anchor());
        break;

    case Graph::TEXT: {
        // box border
        painter->setPen(QPen(color, 1, Qt::DashLine));
        painter->drawRect(resizer_.rect());
        // painter->drawLine(resizer_.rotateAnchor().center(), resizer_.topAnchor().center());

        //
        painter->setBrush(Qt::white);
        painter->setPen(QPen(color, 1, Qt::SolidLine));
        // painter->drawEllipse(resizer_.rotateAnchor());
        painter->drawRects(resizer_.anchors());
        break;
    }

    default: break;
    }

    painter->restore();
}

void PaintCommand::draw_modified(QPainter *painter) { draw(painter, true); }

void PaintCommand::repaint(QPainter *painter) { draw(painter, false); }

bool PaintCommand::regularized()
{
    switch (graph_) {
    case Graph::RECTANGLE:
    case Graph::ELLIPSE: return resizer_.width() == resizer_.height();
    case Graph::LINE:
    case Graph::ARROW: return (resizer_.x1() == resizer_.x2()) || (resizer_.y1() == resizer_.y2());
    default: return true;
    }
}

void PaintCommand::regularize()
{
    switch (graph()) {
    case Graph::ELLIPSE:
    case Graph::RECTANGLE: {
        int width = std::sqrt(resizer_.width() * resizer_.height());
        QRect rect{ QPoint{ 0, 0 }, QPoint{ width, width } };
        rect.moveCenter(resizer_.rect().center());

        resizer_.x1(rect.topLeft().x());
        resizer_.y1(rect.topLeft().y());

        push_point(rect.bottomRight());

        break;
    }
    case Graph::LINE:
    case Graph::ARROW: {
        QPointF p1 = resizer_.point1();
        QPointF p2 = resizer_.point2();

        // 0~180
        auto theta = std::abs(std::atan2(p1.y() - p2.y(), p1.x() - p2.x()) * 180.0 / std::numbers::pi);

        if (std::abs(theta - 90) < 10) {
            p2.setX(p1.x());
        }
        else if (theta < 10 || std::abs(theta - 180) < 10) {
            p2.setY(p1.y());
        }

        push_point(p2.toPoint());

        break;
    }
    default: break;
    }
}

Resizer::PointPosition PaintCommand::hover(const QPoint& pos)
{
    switch (graph_) {
    case RECTANGLE: return resizer().absolutePos(pos);
    case ELLIPSE: {
        QRegion r1(rect().adjusted(-2, -2, 2, 2), QRegion::Ellipse);
        QRegion r2(rect().adjusted(2, 2, -2, -2), QRegion::Ellipse);

        auto hover_pos = resizer_.absolutePos(pos);
        if (!(hover_pos & Resizer::ADJUST_AREA) && r1.contains(pos) && !r2.contains(pos)) {
            return Resizer::BORDER;
        }

        return hover_pos;
    }
    case LINE:
    case ARROW: {
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
        return Resizer::DEFAULT;
    }
    case TEXT: return resizer().absolutePos(pos);
    default: return Resizer::DEFAULT;
    }
}

QCursor PaintCommand::getCursorShapeByHoverPos(Resizer::PointPosition pos, const QCursor& default_cursor)
{
    switch (pos) {
    // clang-format off
    case Resizer::X1_ANCHOR:
    case Resizer::X2_ANCHOR:     return Qt::SizeHorCursor;
    case Resizer::Y1_ANCHOR:
    case Resizer::Y2_ANCHOR:     return Qt::SizeVerCursor;
    case Resizer::X1Y1_ANCHOR:
    case Resizer::X2Y2_ANCHOR:   return Qt::SizeFDiagCursor;
    case Resizer::X1Y2_ANCHOR:
    case Resizer::X2Y1_ANCHOR:   return Qt::SizeBDiagCursor;

    case Resizer::BORDER:
    case Resizer::X1_BORDER:
    case Resizer::X2_BORDER:
    case Resizer::Y1_BORDER:
    case Resizer::Y2_BORDER:     return Qt::SizeAllCursor;
    case Resizer::ROTATE_ANCHOR: return QCursor{ QPixmap(":/icon/res/rotate").scaled(21, 21, Qt::IgnoreAspectRatio, Qt::SmoothTransformation) };

    default:                     return default_cursor;
        // clang-format on
    }
}