#include "selector.h"
#include <QMouseEvent>
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <QShortcut>
#include "detectwidgets.h"

Selector::Selector(QWidget * parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setWindowState(Qt::WindowActive | Qt::WindowFullScreen);

    connect(this, &Selector::moved, [&](){ this->update(); });
    connect(this, &Selector::resized, [&](){ this->update(); });

    registerShortcuts();

    info_ = new Info(this);
    info_->hide();
}

void Selector::start()
{
    if(status_ == INITIAL) {
        status_ = NORMAL;
        show();

        auto rect = DetectWidgets::window();
        begin_ = rect.topLeft();
        end_ = rect.bottomRight();
    }
}

void Selector::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton) {
        cursor_pos_ = position(event->pos());

        switch (status_) {
        case NORMAL:
            begin_ = event->pos();
            end_ = event->pos();

            status_ = SELECTING;
            break;

        case SELECTING: break;

        case CAPTURED:
            if(cursor_pos_ == INSIDE) {
                mbegin_ = event->pos();
                mend_ = event->pos();

                status_ = MOVING;
            }
            else {
                rbegin_ = event->pos();
                rend_ = event->pos();

                status_ = RESIZING;
            }
            break;

        case MOVING:
        case RESIZING:
        case LOCKED:
        default: break;
        }
    }

    QWidget::mousePressEvent(event);
}

void Selector::mouseMoveEvent(QMouseEvent* event)
{
    auto mouse_pos = event->pos();

    switch (status_) {
    case NORMAL: setCursor(Qt::CrossCursor); break;
    case SELECTING:
        end_ = mouse_pos;

        status_ = SELECTING;
        break;

    case CAPTURED:
        cursor_pos_ = position(mouse_pos);
        switch (cursor_pos_) {
        case INSIDE:  setCursor(Qt::SizeAllCursor); break;
        case OUTSIDE: setCursor(Qt::ForbiddenCursor); break;

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
        break;

    case MOVING:
        mend_ = mouse_pos;
        updateSelected();
        mbegin_ = mouse_pos;

        status_ = MOVING;
        break;

    case RESIZING:
        rend_ = mouse_pos;
        updateSelected();
        rbegin_ = mouse_pos;

        status_ = RESIZING;
        break;

    case LOCKED:
    default: break;
    }

    this->update();
    QWidget::mouseMoveEvent(event);
}

void Selector::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton) {
        switch (status_) {
        case NORMAL:break;
        case SELECTING:
            // detected window
            if(event->pos() == begin_) {
                auto window = DetectWidgets::window();
                begin_ = window.topLeft();
                end_ = window.bottomRight();
                status_ = CAPTURED;
            }
            // selected window
            else {
                end_ = event->pos(); status_ = CAPTURED;
            }
            break;
        case MOVING: mend_ = event->pos(); status_ = CAPTURED; break;
        case RESIZING: rend_ = event->pos(); status_ = CAPTURED; break;
        case CAPTURED:
        case LOCKED:
        default: break;
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void Selector::keyPressEvent(QKeyEvent *event)
{
    auto key = event->key();

    if(key == Qt::Key_Escape) {
        status_ = INITIAL;
        end_ = begin_ = {0, 0};
        hide();
    }

    QWidget::keyPressEvent(event);
}

void Selector::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    //
    info_->show();
    info_->move(lt());

    painter_.begin(this);

    if(status_ == NORMAL) {
        auto rect = DetectWidgets::window();
        begin_ = rect.topLeft();
        end_ = rect.bottomRight();
    }

    painter_.setPen(QPen(border_color_, border_width_, border_style_));
    painter_.drawRect(selected());

    painter_.end();

    // info
    info_->size(selected().size());
    auto info_y = lt().y() - info_->geometry().height();
    info_->move(lt().x() + 1, (info_y < 0 ? lt().y() + 1 : info_y));
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

void Selector::registerShortcuts()
{
    // move
    auto move_up = new QShortcut(QKeySequence("W"), this);
    connect(move_up, &QShortcut::activated, [&]() {
        if(status_ == CAPTURED && begin_.y() > 1 && end_.y() > 1) {
            begin_.setY(begin_.y() - 1);
            end_.setY(end_.y() - 1);
            emit moved();
        }
    });

    auto move_down = new QShortcut(QKeySequence("S"), this);
    connect(move_down, &QShortcut::activated, [&]() {
        if(status_ == CAPTURED && begin_.y() + 1 < this->height() && end_.y() + 1 < this->height()) {
            begin_.setY(begin_.y() + 1);
            end_.setY(end_.y() + 1);
            emit moved();
        }
    });

    auto move_left = new QShortcut(QKeySequence("A"), this);
    connect(move_left, &QShortcut::activated, [&]() {
        if(status_ == CAPTURED && begin_.x() > 1 && end_.x() > 1) {
            begin_.setX(begin_.x() - 1);
            end_.setX(end_.x() - 1);
            emit moved();
        }
    });

    auto move_right = new QShortcut(QKeySequence("D"), this);
    connect(move_right, &QShortcut::activated, [&]() {
        if(status_ == CAPTURED && begin_.x() + 1 < this->width() && end_.x() + 1 < this->width()) {
            begin_.setX(begin_.x() + 1);
            end_.setX(end_.x() + 1);
            emit moved();
        }
    });


    // resize
    // increase
    auto increase_top = new QShortcut(Qt::CTRL + Qt::Key_Up, this);
    connect(increase_top, &QShortcut::activated, [&]() {
        if(status_ == CAPTURED) {
            begin_.y() < end_.y()
                    ? begin_.setY(std::max(0, begin_.y() -1))
                    : end_.setY(std::max(0, end_.y() - 1));
            emit resized();
        }
    });

    auto increase_bottom = new QShortcut(Qt::CTRL + Qt::Key_Down, this);
    connect(increase_bottom, &QShortcut::activated, [&]() {
        if(status_ == CAPTURED) {
            begin_.y() < end_.y()
                    ? end_.setY(std::min(this->height(), end_.y() + 1))
                    : begin_.setY(std::min(this->height(), begin_.y() + 1));
            emit resized();
        }
    });

    auto increase_left = new QShortcut(Qt::CTRL + Qt::Key_Left, this);
    connect(increase_left, &QShortcut::activated, [&]() {
        if(status_ == CAPTURED) {
            begin_.x() < end_.x()
                    ? begin_.setX(std::max(0, begin_.x() - 1))
                    : end_.setX(std::max(0, end_.x() - 1));
            emit resized();
        }
    });

    auto increase_right = new QShortcut(Qt::CTRL + Qt::Key_Right, this);
    connect(increase_right, &QShortcut::activated, [&]() {
        if(status_ == CAPTURED) {
            begin_.x() < end_.x()
                    ? end_.setX(std::min(this->width(), end_.x() + 1))
                    : begin_.setX(std::min(this->width(), begin_.x() + 1));
            emit resized();
        }
    });

    // decrease
    auto decrease_top = new QShortcut(Qt::SHIFT + Qt::Key_Up, this);
    connect(decrease_top, &QShortcut::activated, [&]() {
        if(status_ == CAPTURED) {
            begin_.y() > end_.y()
                    ? end_.setY(std::min(this->height(), end_.y() + 1))
                    : begin_.setY(std::min(this->height(), begin_.y() + 1));
            emit resized();
        }
    });

    auto decrease_bottom = new QShortcut(Qt::SHIFT + Qt::Key_Down, this);
    connect(decrease_bottom, &QShortcut::activated, [&]() {
        if(status_ == CAPTURED) {
            begin_.y() > end_.y()
                    ? begin_.setY(std::max(0, begin_.y() - 1))
                    : end_.setY(std::max(0, end_.y() - 1));
            emit resized();
        }
    });

    auto decrease_left = new QShortcut(Qt::SHIFT + Qt::Key_Left, this);
    connect(decrease_left, &QShortcut::activated, [&]() {
        if(status_ == CAPTURED) {
            begin_.x() > end_.x()
                    ? end_.setX(std::min(this->width(), end_.x() + 1))
                    : begin_.setX(std::min(this->width(), begin_.x() + 1));
            emit resized();
        }
    });

    auto decrease_right = new QShortcut(Qt::SHIFT + Qt::Key_Right, this);
    connect(decrease_right, &QShortcut::activated, [&]() {
        if(status_ == CAPTURED) {
            begin_.x() > end_.x()
                    ? begin_.setX(std::max(0, begin_.x() - 1))
                    : end_.setX(std::max(0, end_.x() - 1));
            emit resized();
        }
    });
}

void Selector::setBorderColor(const QColor &c)
{
    border_color_ = c;
}

void Selector::setBorderWidth(int w)
{
    border_width_ = w;
}

void Selector::setBorderStyle(Qt::PenStyle s)
{
    border_style_ = s;
}
