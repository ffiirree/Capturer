#include "selector.h"
#include <QMouseEvent>
#include <QDebug>

Selector::Selector(QWidget * parent)
    : QWidget(parent)
{
    this->setMouseTracking(true);
    this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    this->setWindowState(Qt::WindowActive | Qt::WindowFullScreen);
}


void Selector::start()
{
    if(status_ == INITIAL) {
        status_ = NORMAL;
        this->show();
    }
}

void Selector::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton) {
        cursor_pos_ = position(event->pos());

        switch (cursor_pos_) {
        case INSIDE:
            status_ = MOVING;
            mbegin_ = event->pos();
            mend_ = event->pos();
            break;

        case OUTSIDE:
            status_ = SELECTING;
            begin_ = event->pos();
            end_ = event->pos();
            break;

        default:
            status_ = RESIZING;
            rbegin_ = event->pos();
            rend_ = event->pos();
            break;
        }
    }

    QWidget::mousePressEvent(event);
}

void Selector::mouseMoveEvent(QMouseEvent* event)
{
    auto mouse_pos = event->pos();

    switch (status_) {
    case MOVING:
        mend_ = mouse_pos;
        updateSelected();
        mbegin_ = mouse_pos;
        break;

    case SELECTING:
        end_ = mouse_pos;
        break;

    case RESIZING:
        rend_ = mouse_pos;
        updateSelected();
        rbegin_ = mouse_pos;
        break;

    default: break;
    }

    cursor_pos_ = position(mouse_pos);

    switch (cursor_pos_) {
    case INSIDE:  setCursor(Qt::SizeAllCursor); break;
    case OUTSIDE: setCursor(Qt::CrossCursor); break;

    case TOP_BORDER:
    case BOTTOM_BORDER: setCursor(Qt::SizeVerCursor); break;

    case LEFT_BORDER:
    case RIGHT_BORDER: setCursor(Qt::SizeHorCursor); break;

    case LT_DIAG:
    case RB_DIAG: setCursor(Qt::SizeFDiagCursor); break;

    case LB_DIAG:
    case RT_DIAG: setCursor(Qt::SizeBDiagCursor);break;

    default: break;
    }
    this->update();

    QWidget::mouseMoveEvent(event);
}

void Selector::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton) {
        switch (status_) {
        case SELECTING: end_ = event->pos(); status_ = SELECTED; break;
        case MOVING: mend_ = event->pos(); status_ = SELECTED; break;
        case RESIZING: rend_ = event->pos(); status_ = SELECTED; break;
        default: break;
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void Selector::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Escape) {
        status_ = INITIAL;
        end_ = begin_ = {0, 0};
        this->hide();
    }

    QWidget::keyPressEvent(event);
}

void Selector::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    painter_.begin(this);

    if(status_ > NORMAL) {
        painter_.setPen(QPen(Qt::cyan, 1, Qt::DashDotLine, Qt::FlatCap));
        painter_.drawRect(selected());
    }

    painter_.end();
}

Selector::PointPosition Selector::position(const QPoint& p)
{
    auto pos = INSIDE;
    if(isTopBorder(p)) pos = TOP_BORDER;
    if(isBottomBorder(p)) pos = BOTTOM_BORDER;

    if(isLeftBorder(p))  {
        if(pos == TOP_BORDER) return LT_DIAG;
        if(pos == BOTTOM_BORDER) return LB_DIAG;

        return LEFT_BORDER;
    }
    if(isRightBorder(p)) {
        if(pos == TOP_BORDER) return RT_DIAG;
        if(pos == BOTTOM_BORDER) return RB_DIAG;

        return RIGHT_BORDER;
    }

    if(pos != INSIDE) return pos;

    return isContains(p) ? INSIDE : OUTSIDE;
}

QRect Selector::selected()
{
    auto x = (begin_.x() < end_.x()) ? begin_.x() : end_.x();
    auto y = (begin_.y() < end_.y()) ? begin_.y() : end_.y();

    auto w = std::abs(begin_.x() - end_.x());
    auto h = std::abs(begin_.y() - end_.y());

    // The minimum value is 1
    w = w ? w : 1;
    h = h ? h : 1;

    return QRect(x, y, w, h);
}

void Selector::updateSelected()
{
    if(status_ == MOVING) {
        auto diff_x = mend_.x() - mbegin_.x();
        auto diff_y = mend_.y() - mbegin_.y();

        auto diff_min_x = std::max(std::min(begin_.x(), end_.x()), 0);
        auto diff_max_x = std::max(begin_.x(), end_.x()); diff_max_x = std::max(this->width() - diff_max_x, 0);
        auto diff_min_y = std::max(std::min(begin_.y(), end_.y()), 0);
        auto diff_max_y = std::max(begin_.y(), end_.y()); diff_max_y = std::max(this->height() - diff_max_y, 0);

        diff_x = (diff_x < 0) ? std::max(diff_x, -diff_min_x) : std::min(diff_x, diff_max_x);
        diff_y = (diff_y < 0) ? std::max(diff_y, -diff_min_y) : std::min(diff_y, diff_max_y);

        begin_ = QPoint(begin_.x() + diff_x, begin_.y() + diff_y);
        end_ = QPoint(end_.x() + diff_x, end_.y() + diff_y);
    }
    else if(status_ == RESIZING) {
        auto diff_x = rend_.x() - rbegin_.x();
        auto diff_y = rend_.y() - rbegin_.y();

#define UPDEATE_TOP_BORDER()      (begin_.y() < end_.y() ? begin_.setY(begin_.y() + diff_y) : end_.setY(end_.y() + diff_y))
#define UPDATE_BOTTOM_BORDER()    (begin_.y() > end_.y() ? begin_.setY(begin_.y() + diff_y) : end_.setY(end_.y() + diff_y))
#define UPDATE_RIGHT_BORDER()     (begin_.x() > end_.x() ? begin_.setX(begin_.x() + diff_x) : end_.setX(end_.x() + diff_x))
#define UPDATE_LEFT_BORDER()      (begin_.x() < end_.x() ? begin_.setX(begin_.x() + diff_x) : end_.setX(end_.x() + diff_x))
        switch (cursor_pos_) {
        case TOP_BORDER:     UPDEATE_TOP_BORDER();   break;
        case BOTTOM_BORDER:  UPDATE_BOTTOM_BORDER(); break;
        case RIGHT_BORDER:   UPDATE_RIGHT_BORDER();  break;
        case LEFT_BORDER:    UPDATE_LEFT_BORDER();   break;

        case LT_DIAG: UPDATE_LEFT_BORDER();   UPDEATE_TOP_BORDER();   break;
        case RB_DIAG: UPDATE_RIGHT_BORDER();  UPDATE_BOTTOM_BORDER(); break;
        case LB_DIAG: UPDATE_LEFT_BORDER();   UPDATE_BOTTOM_BORDER(); break;
        case RT_DIAG: UPDATE_RIGHT_BORDER();  UPDEATE_TOP_BORDER();   break;

        default:break;
        }
#undef UPDEATE_TOP_BORDER
#undef UPDATE_BOTTOM_BORDER
#undef UPDATE_RIGHT_BORDER
#undef UPDATE_LEFT_BORDER
    }
}

QPoint Selector::lt()
{
    auto x = (begin_.x() < end_.x()) ? begin_.x() : end_.x();
    auto y = (begin_.y() < end_.y()) ? begin_.y() : end_.y();

    return {x, y};
}

QPoint Selector::rb()
{
    auto x = (begin_.x() > end_.x()) ? begin_.x() : end_.x();
    auto y = (begin_.y() > end_.y()) ? begin_.y() : end_.y();

    return {x, y};
}

bool Selector::isTopBorder(const QPoint& p)
{
    auto sa = selected();
    return QRect(sa.x(), sa.y() - POSITION_BORDER_WIDTH/2, sa.width(), POSITION_BORDER_WIDTH).contains(p);
}
bool Selector::isLeftBorder(const QPoint& p)
{
    auto sa = selected();
    return QRect(sa.x() - POSITION_BORDER_WIDTH/2, sa.y(), POSITION_BORDER_WIDTH, sa.height()).contains(p);
}
bool Selector::isRightBorder(const QPoint& p)
{
    auto sa = selected();
    return QRect(sa.x() + sa.width() - POSITION_BORDER_WIDTH/2, sa.y(), POSITION_BORDER_WIDTH, sa.height()).contains(p);
}
bool Selector::isBottomBorder(const QPoint& p)
{
    auto sa = selected();
    return QRect(sa.x(), sa.y() + sa.height() - POSITION_BORDER_WIDTH/2, sa.width(), POSITION_BORDER_WIDTH).contains(p);
}
