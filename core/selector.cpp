#include "selector.h"
#include <QMouseEvent>
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <QApplication>
#include <QShortcut>
#include <QDesktopWidget>
#include "utils.h"
#include "logging.h"
#include "detectwidgets.h"

Selector::Selector(QWidget * parent)
    : QWidget(parent)
{
    info_ = new SizeInfoWidget(this);

    setAttribute(Qt::WA_TranslucentBackground);

    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::BypassWindowManagerHint);
    setFixedSize(DisplayInfo::maxSize());

    connect(&DisplayInfo::instance(), &DisplayInfo::changed, [this](){ setFixedSize(DisplayInfo::maxSize()); });

    connect(this, SIGNAL(moved()), this, SLOT(update()));
    connect(this, SIGNAL(resized()), this, SLOT(update()));

    registerShortcuts();
}

void Selector::start()
{
    if(status_ == INITIAL) {
        status_ = NORMAL;
        setMouseTracking(true);

        if(use_detect_) {
            DetectWidgets::reset();
            box_.reset(DetectWidgets::window());
            info_->show();
        }

        show();
        activateWindow(); //  Qt::BypassWindowManagerhint: no keyboard input unless call QWidget::activateWindow()
    }
}

void Selector::mousePressEvent(QMouseEvent *event)
{
    auto pos = event->pos();

    if(event->button() == Qt::LeftButton && status_ != LOCKED) {
        cursor_pos_ = box_.absolutePos(pos);

        switch (status_) {
        case NORMAL:
            box_.reset(pos, pos);
            info_->show();

            status_ = SELECTING;
            break;

        case CAPTURED:
            if(cursor_pos_ == Resizer::INSIDE) {
                mbegin_ = mend_ = pos;

                status_ = MOVING;
            }
            else if(cursor_pos_ & Resizer::ADJUST_AREA){
                rbegin_ = rend_ = pos;

                status_ = RESIZING;
            }
            break;

        case SELECTING:
        case MOVING:
        case RESIZING:
        default: LOG(ERROR) << "error status"; break;
        }
    }
}

void Selector::mouseMoveEvent(QMouseEvent* event)
{
    auto mouse_pos = event->pos();

    switch (status_) {
    case NORMAL:
		if (use_detect_) {
			box_.reset(DetectWidgets::window());
            update();
		}
		setCursor(Qt::CrossCursor);
		break;

    case SELECTING:
        box_.x2(mouse_pos.x());
        box_.y2(mouse_pos.y());

        status_ = SELECTING;
        update();
        break;

    case CAPTURED:
        switch (box_.relativePos(mouse_pos)) {
        case Resizer::INSIDE:  setCursor(Qt::SizeAllCursor); break;
        case Resizer::OUTSIDE: setCursor(Qt::ForbiddenCursor); break;

        case Resizer::T_ANCHOR:
        case Resizer::B_ANCHOR:
        case Resizer::T_BORDER:
        case Resizer::B_BORDER: setCursor(Qt::SizeVerCursor); break;

        case Resizer::L_ANCHOR:
        case Resizer::R_ANCHOR:
        case Resizer::L_BORDER:
        case Resizer::R_BORDER: setCursor(Qt::SizeHorCursor); break;

        case Resizer::TL_ANCHOR:
        case Resizer::BR_ANCHOR: setCursor(Qt::SizeFDiagCursor); break;
        case Resizer::BL_ANCHOR:
        case Resizer::TR_ANCHOR: setCursor(Qt::SizeBDiagCursor); break;
        default: break;
        }
        break;

    case MOVING:
        mend_ = mouse_pos;
        updateSelected();
        mbegin_ = mouse_pos;

        update();
        emit moved();
        break;

    case RESIZING:
        rend_ = mouse_pos;
        updateSelected();
        rbegin_ = mouse_pos;

        update();
        emit resized();
        break;

    case LOCKED:
    default: break;
    }
}

void Selector::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton) {
        switch (status_) {
        case NORMAL:break;
        case SELECTING:
            // detected window
            if(box_.width() == 1 && box_.height() == 1 && use_detect_) {
                box_.reset(DetectWidgets::window());
            }
            CAPTURED();
            break;
        case MOVING:
        case RESIZING:  CAPTURED(); break;
        case CAPTURED:
        case LOCKED:
        default: break;
        }
    }
}

void Selector::exit()
{
    status_ = INITIAL;
    box_.reset(0, 0, 0, 0);
    info_->hide();

    repaint();
    setMouseTracking(false);

    QWidget::hide();
}

void Selector::drawSelector(QPainter *painter, const Resizer& resizer)
{
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setBrush(Qt::NoBrush);

    // box border
    painter->setPen(QPen(Qt::black, 1, Qt::DashLine));
    painter->drawRect(resizer.rect());

    // anchors
    painter->setPen(QPen(Qt::black, 1, Qt::SolidLine));
    painter->drawRects(resizer.anchors());
}

void Selector::paintEvent(QPaintEvent *)
{
    painter_.begin(this);

    auto srect = selected();

    painter_.fillRect(QRect{ 0, 0, width(), srect.y() }, mask_color_);
    painter_.fillRect(QRect{ 0, srect.y(), srect.x(), srect.height() }, mask_color_);
    painter_.fillRect(QRect{ srect.x() + srect.width(), srect.y(), width() - srect.x() - srect.width(), srect.height()}, mask_color_);
    painter_.fillRect(QRect{ 0, srect.y() + srect.height(), width(), height() - srect.y() - srect.height()}, mask_color_);

    if(use_detect_ || status_ > NORMAL) {
        // info
        info_->size(selected().size());
        auto info_y = box_.top() - info_->geometry().height();
        info_->move(box_.left() + 1, (info_y < 0 ? box_.top() + 1 : info_y));

        // draw border
        painter_.setPen(pen_);
        painter_.setBrush(QColor(0, 0, 0, 1));
        painter_.drawRect(srect);

        // draw anchors
        painter_.setBrush(pen_.color());
        painter_.drawRects(box_.anchors());
    }
    painter_.end();

    modified_ = PaintType::UNMODIFIED;
}

void Selector::updateSelected()
{
    if(status_ == MOVING) {
        box_.move(mend_.x() - mbegin_.x(), mend_.y() - mbegin_.y());
    }
    else if(status_ == RESIZING) {
        auto diff_x = rend_.x() - rbegin_.x();
        auto diff_y = rend_.y() - rbegin_.y();

        switch (cursor_pos_) {
        case Resizer::Y1_BORDER: case Resizer::Y1_ANCHOR: box_.ry1() += diff_y; break;
        case Resizer::Y2_BORDER: case Resizer::Y2_ANCHOR: box_.ry2() += diff_y; break;
        case Resizer::X1_BORDER: case Resizer::X1_ANCHOR: box_.rx1() += diff_x; break;
        case Resizer::X2_BORDER: case Resizer::X2_ANCHOR: box_.rx2() += diff_x; break;

        case Resizer::X1Y1_ANCHOR: box_.rx1() += diff_x; box_.ry1() += diff_y; break;
        case Resizer::X1Y2_ANCHOR: box_.rx1() += diff_x; box_.ry2() += diff_y; break;
        case Resizer::X2Y1_ANCHOR: box_.rx2() += diff_x; box_.ry1() += diff_y; break;
        case Resizer::X2Y2_ANCHOR: box_.rx2() += diff_x; box_.ry2() += diff_y; break;

        default:break;
        }
    }
}

void Selector::registerShortcuts()
{
    // move
    auto move_up = new QShortcut(QKeySequence("W"), this);
    connect(move_up, &QShortcut::activated, [=]() {
        if(status_ == CAPTURED) {
            box_.move(0, -1);
            emit moved();
        }
    });

    auto move_down = new QShortcut(QKeySequence("S"), this);
    connect(move_down, &QShortcut::activated, [=]() {
        if(status_ == CAPTURED) {
            box_.move(0, 1);
            emit moved();
        }
    });

    auto move_left = new QShortcut(QKeySequence("A"), this);
    connect(move_left, &QShortcut::activated, [=]() {
        if(status_ == CAPTURED) {
            box_.move(-1, 0);
            emit moved();
        }
    });

    auto move_right = new QShortcut(QKeySequence("D"), this);
    connect(move_right, &QShortcut::activated, [=]() {
        if(status_ == CAPTURED) {
            box_.move(1, 0);
            emit moved();
        }
    });

    // resize
    // increase
    auto increase_top = new QShortcut(Qt::CTRL + Qt::Key_Up, this);
    connect(increase_top, &QShortcut::activated, [=]() {
        if(status_ == CAPTURED) {
            box_.y1() < box_.y2() ? box_.ry1() = std::max(box_.y1() - 1, 0) : box_.ry2() = std::max(box_.y2() - 1, 0);
            emit resized();
        }
    });

    auto increase_bottom = new QShortcut(Qt::CTRL + Qt::Key_Down, this);
    connect(increase_bottom, &QShortcut::activated, [=]() {
        if(status_ == CAPTURED) {
            box_.y1() > box_.y2() ? box_.ry1() = std::min(box_.y1() + 1, height()) : box_.ry2() = std::min(box_.y2() + 1, height());
            emit resized();
        }
    });

    auto increase_left = new QShortcut(Qt::CTRL + Qt::Key_Left, this);
    connect(increase_left, &QShortcut::activated, [=]() {
        if(status_ == CAPTURED) {
            box_.x1() < box_.x2() ? box_.rx1() = std::max(box_.x1() - 1, 0) : box_.rx2() = std::max(box_.x2() - 1, 0);
            emit resized();
        }
    });

    auto increase_right = new QShortcut(Qt::CTRL + Qt::Key_Right, this);
    connect(increase_right, &QShortcut::activated, [=]() {
        if(status_ == CAPTURED) {
            box_.x1() > box_.x2() ? box_.rx1() = std::min(box_.x1() + 1, width()) : box_.rx2() = std::min(box_.x2() + 1, width());
            emit resized();
        }
    });

    // decrease
    auto decrease_top = new QShortcut(Qt::SHIFT + Qt::Key_Up, this);
    connect(decrease_top, &QShortcut::activated, [=]() {
        if(status_ == CAPTURED) {
            box_.y1()< box_.y2()? box_.ry1() = std::min(box_.y1()+ 1, box_.y2()) : box_.ry2() = std::min(box_.y2()+ 1, box_.y1());
            emit resized();
        }
    });

    auto decrease_bottom = new QShortcut(Qt::SHIFT + Qt::Key_Down, this);
    connect(decrease_bottom, &QShortcut::activated, [=]() {
        if(status_ == CAPTURED) {
            box_.y1()> box_.y2()? box_.ry1()= std::max(box_.y1()- 1, box_.y2()) : box_.ry2() = std::max(box_.y2()- 1, box_.y1());
            emit resized();
        }
    });

    auto decrease_left = new QShortcut(Qt::SHIFT + Qt::Key_Left, this);
    connect(decrease_left, &QShortcut::activated, [=]() {
        if(status_ == CAPTURED) {
            box_.x1() < box_.x2()? box_.rx1() = std::min(box_.x1() + 1, box_.x2()) : box_.rx2()= std::min(box_.x2()+ 1, box_.x1());
            emit resized();
        }
    });

    auto decrease_right = new QShortcut(Qt::SHIFT + Qt::Key_Right, this);
    connect(decrease_right, &QShortcut::activated, [=]() {
        if(status_ == CAPTURED) {
            box_.x1() > box_.x2()? box_.rx1() = std::max(box_.x1() - 1, box_.x2()) : box_.rx2() = std::max(box_.x2()- 1, box_.x1());
            emit resized();
        }
    });

    auto select_all = new QShortcut(Qt::CTRL + Qt::Key_A, this);
    connect(select_all, &QShortcut::activated, [=]() {
        if(status_ <= CAPTURED) {
            box_.reset(rect());
            emit resized();
            CAPTURED();
        }
    });
}

void Selector::updateTheme(json& setting)
{
    setBorderColor(setting["border"]["color"].get<QColor>());
    setBorderWidth(setting["border"]["width"].get<int>());
    setBorderStyle(setting["border"]["style"].get<Qt::PenStyle>());
    setMaskColor(setting["mask"]["color"].get<QColor>());
}

void Selector::setBorderColor(const QColor &c)
{
    pen_.setColor(c);
}

void Selector::setBorderWidth(int w)
{
    pen_.setWidth(w);
}

void Selector::setBorderStyle(Qt::PenStyle s)
{
    pen_.setStyle(s);
}

void Selector::setMaskColor(const QColor& c)
{
    mask_color_ = c;
}

void Selector::setUseDetectWindow(bool f)
{
    use_detect_ = f;
}
