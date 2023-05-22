#include "selector.h"

#include "logging.h"

#include <fmt/core.h>
#include <QApplication>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QScreen>
#include <QShortcut>

Selector::Selector(QWidget *parent)
    : QWidget(parent)
{
    info_ = new QLabel(this);
    info_->setMinimumHeight(24);
    info_->setMinimumWidth(100);
    info_->setAlignment(Qt::AlignCenter);
    info_->setObjectName("size_info");
    info_->setVisible(false);

    setAttribute(Qt::WA_TranslucentBackground);

    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                   Qt::BypassWindowManagerHint);

    connect(this, &Selector::moved, [this]() { update(); });
    connect(this, &Selector::resized, [this]() { update(); });

    registerShortcuts();
}

void Selector::start()
{
    if (status_ == SelectorStatus::INITIAL) {
        status_ = SelectorStatus::NORMAL;
        setMouseTracking(true);

        if (use_detect_) {
            hunter::ready(windows_detection_flags_);
            select(hunter::hunt(QCursor::pos()));
            info_->show();
        }

        setGeometry(probe::graphics::virtual_screen_geometry());
        show();
        activateWindow(); //  Qt::BypassWindowManagerHint: no keyboard input unless call
                          //  QWidget::activateWindow()
    }
}

void Selector::exit()
{
    status_ = SelectorStatus::INITIAL;
    setMouseTracking(false);

    mask_hidden_ = false;
    hide();
}

void Selector::mousePressEvent(QMouseEvent *event)
{
    auto pos = event->globalPos();

    if (event->button() == Qt::LeftButton && status_ != SelectorStatus::LOCKED) {
        cursor_pos_ = box_.absolutePos(pos);

        switch (status_) {
        case SelectorStatus::NORMAL: sbegin_ = pos; status_ = SelectorStatus::START_SELECTING;
#ifdef _WIN32
            if (auto display = probe::graphics::display_contains(pos);
                scope_ == scope_t::display && display.has_value()) {
                box_.range(display.value().geometry);
            }
#endif
            break;

        case SelectorStatus::CAPTURED:
            if (cursor_pos_ == Resizer::INSIDE) {
                mbegin_ = mend_ = pos;

                status_ = SelectorStatus::MOVING;
            }
            else if (cursor_pos_ & Resizer::ADJUST_AREA) {
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

void Selector::mouseMoveEvent(QMouseEvent *event)
{
    auto mouse_pos = event->globalPos();

    switch (status_) {
    case SelectorStatus::NORMAL:
        if (use_detect_) {
            select(hunter::hunt(QCursor::pos()));
        }
        setCursor(Qt::CrossCursor);
        break;

    case SelectorStatus::START_SELECTING:
        if (std::abs(mouse_pos.x() - sbegin_.x()) >= std::min(min_size_.width(), 4) &&
            std::abs(mouse_pos.y() - sbegin_.y()) >= std::min(min_size_.height(), 4)) {
            select({ sbegin_, mouse_pos });
            info_->show();
            status_ = SelectorStatus::SELECTING;
        }
        break;

    case SelectorStatus::SELECTING:
        adjust(0, 0, mouse_pos.x() - box_.x2(), mouse_pos.y() - box_.y2());

        status_ = SelectorStatus::SELECTING;
        break;

    case SelectorStatus::CAPTURED:
        // clang-format off
        switch (box_.relativePos(mouse_pos)) {
        case Resizer::INSIDE:       setCursor(Qt::SizeAllCursor); break;
        case Resizer::OUTSIDE:      setCursor(Qt::ForbiddenCursor); break;

        case Resizer::T_ANCHOR:
        case Resizer::B_ANCHOR:
        case Resizer::T_BORDER:
        case Resizer::B_BORDER:     setCursor(Qt::SizeVerCursor); break;

        case Resizer::L_ANCHOR:
        case Resizer::R_ANCHOR:
        case Resizer::L_BORDER:
        case Resizer::R_BORDER:     setCursor(Qt::SizeHorCursor); break;

        case Resizer::TL_ANCHOR:
        case Resizer::BR_ANCHOR:    setCursor(Qt::SizeFDiagCursor); break;
        case Resizer::BL_ANCHOR:
        case Resizer::TR_ANCHOR:    setCursor(Qt::SizeBDiagCursor); break;
        default: break;
            // clang-format on
        }
        break;

    case SelectorStatus::MOVING: {
        mend_ = mouse_pos;

        translate(mend_.x() - mbegin_.x(), mend_.y() - mbegin_.y());

        mbegin_ = mouse_pos;

        setCursor(Qt::SizeAllCursor);
        break;
    }

    case SelectorStatus::RESIZING:
        // clang-format off
        switch (cursor_pos_) {
        case Resizer::Y1_BORDER: case Resizer::Y1_ANCHOR: adjust(0, mouse_pos.y() - box_.y1(), 0, 0); break;
        case Resizer::Y2_BORDER: case Resizer::Y2_ANCHOR: adjust(0, 0, 0, mouse_pos.y() - box_.y2()); break;
        case Resizer::X1_BORDER: case Resizer::X1_ANCHOR: adjust(mouse_pos.x() - box_.x1(), 0, 0, 0); break;
        case Resizer::X2_BORDER: case Resizer::X2_ANCHOR: adjust(0, 0, mouse_pos.x() - box_.x2(), 0); break;

        case Resizer::X1Y1_ANCHOR: adjust(mouse_pos.x() - box_.x1(), mouse_pos.y() - box_.y1(), 0, 0); break;
        case Resizer::X1Y2_ANCHOR: adjust(mouse_pos.x() - box_.x1(), 0, 0, mouse_pos.y() - box_.y2()); break;
        case Resizer::X2Y1_ANCHOR: adjust(0, mouse_pos.y() - box_.y1(), mouse_pos.x() - box_.x2(), 0); break;
        case Resizer::X2Y2_ANCHOR: adjust(0, 0, mouse_pos.x() - box_.x2(), mouse_pos.y() - box_.y2()); break;

        default: break;
        }
        // clang-format on
        break;

    case SelectorStatus::LOCKED:
    default: break;
    }
}

void Selector::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        switch (status_) {
        case SelectorStatus::NORMAL: break;
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
            if (!isValid()) {
                if (use_detect_) { // detected window
                    select(hunter::hunt(QCursor::pos()));
                    CAPTURED();
                }
                else { // reset
                    info_->hide();
                    select(QRect(0, 0, 0, 0));
                    status_ = SelectorStatus::NORMAL;
                }
            }
            else {
                CAPTURED();
            }
            break;
        case SelectorStatus::MOVING:
        case SelectorStatus::RESIZING: CAPTURED(); break;
        case SelectorStatus::CAPTURED:
        case SelectorStatus::LOCKED:
        default: break;
        }
    }
}

void Selector::update_info_label()
{
    std::string str{ "-- x --" };

    if (isValid()) {
        str = fmt::format("{} x {}", selected().width(), selected().height());
        if (prey_.type == hunter::prey_type_t::window || prey_.type == hunter::prey_type_t::desktop) {
            str += prey_.name.empty()
                       ? (prey_.codename.empty() ? std::string{} : " : [" + prey_.codename + "]")
                       : " : " + prey_.name;
        }
        else if (prey_.type == hunter::prey_type_t::display) {
            str += " : " + prey_.codename + " - " + prey_.name;
        }
    }

    info_->setText(QString::fromUtf8(str.c_str()));

    info_->adjustSize();

    // move
    auto info_y = box_.top() - info_->geometry().height() - 1;
    info_->move(QPoint(box_.left() + 1, (info_y < 0 ? box_.top() + 1 : info_y - 1)) -
                QRect(probe::graphics::virtual_screen_geometry()).topLeft());
}

void Selector::paintEvent(QPaintEvent *)
{
    painter_.begin(this);
    painter_.translate(-geometry().topLeft()); // (0, 0) at primary screen (0, 0)
    auto srect = selected();

    if (!mask_hidden_) {
        painter_.save();

        painter_.setBrush(mask_color_);
        painter_.setClipping(true);
        painter_.setClipRegion(QRegion(geometry()).subtracted(QRegion(srect)));
        painter_.drawRect(geometry());
        painter_.setClipping(false);

        painter_.restore();

        if (use_detect_ || status_ > SelectorStatus::NORMAL) {
            // info label
            update_info_label();

            // draw border
            painter_.setPen(pen_);
            if (prevent_transparent_) painter_.setBrush(QColor(0, 0, 0, 1));
            painter_.drawRect(srect.adjusted(-pen_.width() % 2, -pen_.width() % 2, 0, 0));

            // draw anchors
            painter_.setPen({ pen_.color(), pen_.widthF(), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
            painter_.setBrush(Qt::white);
            painter_.drawRects((srect.width() >= 32 && srect.height() >= 32) ? box_.anchors()
                                                                             : box_.cornerAnchors());
        }
    }
    else {
        painter_.setPen(QPen(pen_.color(), 2));
        painter_.drawRect(srect.adjusted(-1, -1, 1, 1));
    }

    painter_.end();
}

void Selector::select(const hunter::prey_t& prey)
{
    box_.coords(prey.geometry);
    prey_ = prey;

    update();
}

void Selector::select(const probe::graphics::display_t& dis)
{
    box_.coords(dis.geometry);
    prey_ = hunter::prey_t::from(dis);

    update();
}

void Selector::select(const QRect& rect)
{
    box_.coords(rect);
    prey_ = hunter::prey_t::from(rect);

    update();
}

void Selector::translate(int32_t dx, int32_t dy)
{
    if (status_ != SelectorStatus::CAPTURED && status_ != SelectorStatus::MOVING) return;

    box_.translate(dx, dy);
    prey_ = hunter::prey_t::from(box_.rect());

    emit moved();
}

void Selector::adjust(int32_t dx1, int32_t dy1, int32_t dx2, int32_t dy2)
{
    if (status_ != SelectorStatus::CAPTURED && status_ != SelectorStatus::RESIZING &&
        status_ != SelectorStatus::SELECTING)
        return;

    box_.adjust(dx1, dy1, dx2, dy2);
    prey_ = hunter::prey_t::from(box_.rect());

    emit resized();
}

void Selector::margins(int32_t t, int32_t r, int32_t b, int32_t l)
{
    if (status_ != SelectorStatus::CAPTURED && status_ != SelectorStatus::RESIZING) return;

    box_.margins(t, r, b, l);
    prey_ = hunter::prey_t::from(box_.rect());

    emit resized();
}

void Selector::registerShortcuts()
{
    // clang-format off
    // move 1 pixel
    connect(new QShortcut(Qt::Key_W,     this), &QShortcut::activated, [this]() { translate(0, -1); });
    connect(new QShortcut(Qt::Key_Up,    this), &QShortcut::activated, [this]() { translate(0, -1); });

    connect(new QShortcut(Qt::Key_S,     this), &QShortcut::activated, [this]() { translate(0, +1); });
    connect(new QShortcut(Qt::Key_Down,  this), &QShortcut::activated, [this]() { translate(0, +1); });

    connect(new QShortcut(Qt::Key_A,     this), &QShortcut::activated, [this]() { translate(-1, 0); });
    connect(new QShortcut(Qt::Key_Left,  this), &QShortcut::activated, [this]() { translate(-1, 0); });

    connect(new QShortcut(Qt::Key_D,     this), &QShortcut::activated, [this]() { translate(+1, 0); });
    connect(new QShortcut(Qt::Key_Right, this), &QShortcut::activated, [this]() { translate(+1, 0); });

    // resize
    // expand 1 pixel
    connect(new QShortcut(Qt::CTRL | Qt::Key_Up,    this), &QShortcut::activated, [this]() { margins(-1, 0, 0, 0); });
    connect(new QShortcut(Qt::CTRL | Qt::Key_Down,  this), &QShortcut::activated, [this]() { margins(0, 0, +1, 0); });
    connect(new QShortcut(Qt::CTRL | Qt::Key_Left,  this), &QShortcut::activated, [this]() { margins(0, 0, 0, -1); });
    connect(new QShortcut(Qt::CTRL | Qt::Key_Right, this), &QShortcut::activated, [this]() { margins(0, +1, 0, 0); });

    // shrink 1 pixel
    connect(new QShortcut(Qt::SHIFT | Qt::Key_Up,   this), &QShortcut::activated, [this]() { margins(+1, 0, 0, 0); });
    connect(new QShortcut(Qt::SHIFT | Qt::Key_Down, this), &QShortcut::activated, [this]() { margins(0, 0, -1, 0); });
    connect(new QShortcut(Qt::SHIFT | Qt::Key_Left, this), &QShortcut::activated, [this]() { margins(0, 0, 0, +1); });
    connect(new QShortcut(Qt::SHIFT | Qt::Key_Right,this), &QShortcut::activated, [this]() { margins(0, -1, 0, 0); });
    // clang-format on

    // fullscreen
    connect(new QShortcut(Qt::CTRL | Qt::Key_A, this), &QShortcut::activated, [this]() {
        if (status_ <= SelectorStatus::CAPTURED) {

            // rectangle / window -> display
            if (prey_.type < hunter::prey_type_t::display) {
                for (const auto& display : probe::graphics::displays()) {
                    if (QRect(display.geometry).contains(box_.rect(), false)) {
                        select(display);
                    }
                }
            }
            else if (prey_.type == hunter::prey_type_t::display && scope_ == scope_t::desktop) {
                auto desktop = probe::graphics::virtual_screen();
                select(hunter::prey_t{
                    .type     = hunter::prey_type_t::desktop,
                    .geometry = desktop.geometry,
                    .handle   = desktop.handle,
                    .name     = desktop.name,
                    .codename = desktop.classname,
                });
            }

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

void Selector::setBorderColor(const QColor& c) { pen_.setColor(c); }

void Selector::setBorderWidth(int w) { pen_.setWidth(w); }

void Selector::setBorderStyle(Qt::PenStyle s) { pen_.setStyle(s); }

void Selector::setMaskColor(const QColor& c) { mask_color_ = c; }

void Selector::setUseDetectWindow(bool f) { use_detect_ = f; }

std::string to_string(Selector::scope_t s)
{
    switch (s) {
    case Selector::scope_t::desktop: return "desktop";
    case Selector::scope_t::display: return "display";

    default: return "unknown";
    }
}