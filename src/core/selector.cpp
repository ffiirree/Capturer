#include "selector.h"
#include <QMouseEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QApplication>
#include <QShortcut>
#include <QDesktopWidget>
#include <fmt/core.h>
#include "utils.h"
#include "logging.h"
#include "widgetsdetector.h"

Selector::Selector(QWidget * parent)
    : QWidget(parent)
{
    info_ = new QLabel(this);
    info_->setFixedSize(100, 24);
    info_->setAlignment(Qt::AlignCenter);
    info_->setObjectName("size_info");
    info_->setVisible(false);

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
    if(status_ == SelectorStatus::INITIAL) {
        status_ = SelectorStatus::NORMAL;
        setMouseTracking(true);

        if(use_detect_) {
            WidgetsDetector::refresh();
            box_.reset(WidgetsDetector::window_rect());
            info_->show();
        }

        show();
        activateWindow(); //  Qt::BypassWindowManagerhint: no keyboard input unless call QWidget::activateWindow()
    }
}

void Selector::exit()
{
    status_ = SelectorStatus::INITIAL;
    setMouseTracking(false);

    mask_hidded_ = false;
    hide();
}

void Selector::mousePressEvent(QMouseEvent *event)
{
    auto pos = event->pos();

    if(event->button() == Qt::LeftButton && status_ != SelectorStatus::LOCKED) {
        cursor_pos_ = box_.absolutePos(pos);

        switch (status_) {
        case SelectorStatus::NORMAL:
            sbegin_ = pos;
            status_ = SelectorStatus::START_SELECTING;
            break;

        case SelectorStatus::CAPTURED:
            if(cursor_pos_ == Resizer::INSIDE) {
                mbegin_ = mend_ = pos;

                status_ = SelectorStatus::MOVING;
            }
            else if(cursor_pos_ & Resizer::ADJUST_AREA){
                status_ = SelectorStatus::RESIZING;
            }
            break;

        case SelectorStatus::SELECTING:
        case SelectorStatus::MOVING:
        case SelectorStatus::RESIZING:
        default: LOG(ERROR) << "error status"; break;
        }
    }
}

void Selector::mouseMoveEvent(QMouseEvent* event)
{
    auto mouse_pos = event->pos();

    switch (status_) {
    case SelectorStatus::NORMAL:
        if (use_detect_) {
            box_.reset(WidgetsDetector::window_rect());
            update();
        }
        setCursor(Qt::CrossCursor);
        break;

    case SelectorStatus::START_SELECTING:
        if (std::abs(mouse_pos.x() - sbegin_.x()) >= std::min(min_size_.width(), 4) &&
            std::abs(mouse_pos.y() - sbegin_.y()) >= std::min(min_size_.height(), 4)) {
            box_.reset(sbegin_, mouse_pos);
            info_->show();
            status_ = SelectorStatus::SELECTING;
            update();
        }
        break;

    case SelectorStatus::SELECTING:
        box_.x2(mouse_pos.x());
        box_.y2(mouse_pos.y());

        status_ = SelectorStatus::SELECTING;
        update();
        break;

    case SelectorStatus::CAPTURED:
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

    case SelectorStatus::MOVING:
        mend_ = mouse_pos;
        box_.move(mend_.x() - mbegin_.x(), mend_.y() - mbegin_.y());
        mbegin_ = mouse_pos;

        update();
        emit moved();

        setCursor(Qt::SizeAllCursor);
        break;

    case SelectorStatus::RESIZING:
        switch (cursor_pos_) {
        case Resizer::Y1_BORDER: case Resizer::Y1_ANCHOR: box_.ry1() = mouse_pos.y(); break;
        case Resizer::Y2_BORDER: case Resizer::Y2_ANCHOR: box_.ry2() = mouse_pos.y(); break;
        case Resizer::X1_BORDER: case Resizer::X1_ANCHOR: box_.rx1() = mouse_pos.x(); break;
        case Resizer::X2_BORDER: case Resizer::X2_ANCHOR: box_.rx2() = mouse_pos.x(); break;

        case Resizer::X1Y1_ANCHOR: box_.rx1() = mouse_pos.x(); box_.ry1() = mouse_pos.y(); break;
        case Resizer::X1Y2_ANCHOR: box_.rx1() = mouse_pos.x(); box_.ry2() = mouse_pos.y(); break;
        case Resizer::X2Y1_ANCHOR: box_.rx2() = mouse_pos.x(); box_.ry1() = mouse_pos.y(); break;
        case Resizer::X2Y2_ANCHOR: box_.rx2() = mouse_pos.x(); box_.ry2() = mouse_pos.y(); break;

        default:break;
        }

        update();
        emit resized();
        break;

    case SelectorStatus::LOCKED:
    default: break;
    }
}

void Selector::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton) {
        switch (status_) {
        case SelectorStatus::NORMAL:break;
        case SelectorStatus::START_SELECTING:
            if (use_detect_) {
                CAPTURED();
            }
            else {
                status_ = SelectorStatus::NORMAL;
            }
            break;
        case SelectorStatus::SELECTING:
            // invalid size
            if(!isValid()) {
                if (use_detect_) { // detected window
                    box_.reset(WidgetsDetector::window_rect());
                    CAPTURED();
                }
                else {   // reset
                    info_->hide();
                    box_.reset({ 0,0 }, { 0,0 });
                    status_ = SelectorStatus::NORMAL;
                    update();
                }
            }
            else {
                CAPTURED();
            }
            break;
        case SelectorStatus::MOVING:
        case SelectorStatus::RESIZING:  CAPTURED(); break;
        case SelectorStatus::CAPTURED:
        case SelectorStatus::LOCKED:
        default: break;
        }
    }
}

void Selector::paintEvent(QPaintEvent *)
{
    painter_.begin(this);
    auto srect = selected();

    if (!mask_hidded_) {
        painter_.save();

        painter_.setBrush(mask_color_);
        painter_.setClipping(true);
        painter_.setClipRegion(QRegion(rect()).subtracted(QRegion(srect)));
        painter_.drawRect(rect());
        painter_.setClipping(false);

        painter_.restore();

        if (use_detect_ || status_ > SelectorStatus::NORMAL) {
            // info
            info_->setText(isValid() ? fmt::format("{} x {}", selected().width(), selected().height()).c_str() : "-- x --");
            auto info_y = box_.top() - info_->geometry().height() - 1;
            info_->move(box_.left() + 1, (info_y < 0 ? box_.top() + 1 : info_y - 1));

            // draw border
            painter_.setPen(pen_);
            if (prevent_transparent_)
                painter_.setBrush(QColor(0, 0, 0, 1));
            painter_.drawRect(srect.adjusted(-pen_.width() % 2, -pen_.width() % 2, 0, 0));

            // draw anchors
            painter_.setPen({ pen_.color(), pen_.widthF(), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
            painter_.setBrush(Qt::white);
            painter_.drawRects((srect.width() >= 32 && srect.height() >= 32) ? box_.anchors() : box_.cornerAnchors());
        }
    }
    else {
        painter_.setPen(QPen(pen_.color(), 2));
        painter_.drawRect(srect.adjusted(-1, -1, 1, 1));
    }

    painter_.end();
}

void Selector::moveSelectedBox(int x, int y)
{
    if(status_ == SelectorStatus::CAPTURED) {
        box_.move(x, y);
        emit moved();
    }
}

void Selector::registerShortcuts()
{
    // move
    connect(new QShortcut(Qt::Key_W, this), &QShortcut::activated, [this](){ moveSelectedBox(0, -1); });
    connect(new QShortcut(Qt::Key_Up, this), &QShortcut::activated, [this](){ moveSelectedBox(0, -1); });

    connect(new QShortcut(Qt::Key_S, this), &QShortcut::activated, [this]() { moveSelectedBox(0, 1); });
    connect(new QShortcut(Qt::Key_Down, this), &QShortcut::activated, [this]() { moveSelectedBox(0, 1); });

    connect(new QShortcut(Qt::Key_A, this), &QShortcut::activated, [this]() { moveSelectedBox(-1, 0); });
    connect(new QShortcut(Qt::Key_Left, this), &QShortcut::activated, [this]() { moveSelectedBox(-1, 0); });

    connect(new QShortcut(Qt::Key_D, this), &QShortcut::activated, [this]() { moveSelectedBox(1, 0); });
    connect(new QShortcut(Qt::Key_Right, this), &QShortcut::activated, [this]() { moveSelectedBox(1, 0); });

    // resize
    // increase
    connect(new QShortcut(Qt::CTRL + Qt::Key_Up, this), &QShortcut::activated, [this]() {
        if(status_ == SelectorStatus::CAPTURED) {
            box_.rtop() = std::max(box_.top() - 1, 0);
            emit resized();
        }
    });

    connect(new QShortcut(Qt::CTRL + Qt::Key_Down, this), &QShortcut::activated, [this]() {
        if(status_ == SelectorStatus::CAPTURED) {
            box_.rbottom() = std::min(box_.bottom() + 1, height());
            emit resized();
        }
    });

    connect(new QShortcut(Qt::CTRL + Qt::Key_Left, this), &QShortcut::activated, [this]() {
        if(status_ == SelectorStatus::CAPTURED) {
            box_.rleft() = std::max(box_.left() - 1, 0);
            emit resized();
        }
    });

    connect(new QShortcut(Qt::CTRL + Qt::Key_Right, this), &QShortcut::activated, [this]() {
        if(status_ == SelectorStatus::CAPTURED) {
            box_.rright() = std::min(box_.right() + 1, width());
            emit resized();
        }
    });

    // decrease
    connect(new QShortcut(Qt::SHIFT + Qt::Key_Up, this), &QShortcut::activated, [this]() {
        if(status_ == SelectorStatus::CAPTURED) {
            box_.rtop() = std::min(box_.top() + 1, box_.bottom());
            emit resized();
        }
    });

    connect(new QShortcut(Qt::SHIFT + Qt::Key_Down, this), &QShortcut::activated, [this]() {
        if(status_ == SelectorStatus::CAPTURED) {
            box_.rbottom() = std::max(box_.bottom() - 1, box_.top());
            emit resized();
        }
    });

    connect(new QShortcut(Qt::SHIFT + Qt::Key_Left, this), &QShortcut::activated, [this]() {
        if(status_ == SelectorStatus::CAPTURED) {
            box_.rleft() = std::min(box_.left() + 1, box_.right());
            emit resized();
        }
    });

    connect(new QShortcut(Qt::SHIFT + Qt::Key_Right, this), &QShortcut::activated, [this]() {
        if(status_ == SelectorStatus::CAPTURED) {
            box_.rright() = std::max(box_.right() - 1, box_.left());
            emit resized();
        }
    });

    connect(new QShortcut(Qt::CTRL + Qt::Key_A, this), &QShortcut::activated, [this]() {
        if(status_ <= SelectorStatus::CAPTURED) {
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
