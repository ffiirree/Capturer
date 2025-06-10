#include "selector.h"

#include "logging.h"

#include <fmt/core.h>
#include <QApplication>
#include <QGuiApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QShortcut>

Selector::Selector(QWidget *parent)
    : QWidget(parent)
{
    info_ = new QLabel(this);
    info_->setMinimumSize(100, 24);
    info_->setAlignment(Qt::AlignCenter);
    info_->setObjectName("size_info");
    info_->setVisible(false);

    connect(this, &Selector::moved, [this]() { update(); });
    connect(this, &Selector::resized, [this]() { update(); });

    registerShortcuts();

    status(SelectorStatus::READY);
}

void Selector::status(const SelectorStatus status)
{
    if (status_ == status) return;

    status_ = status;
    setAttribute(Qt::WA_TransparentForMouseEvents, status == SelectorStatus::LOCKED);

    switch (status) {
    case SelectorStatus::PREY_SELECTING:
    case SelectorStatus::FREE_SELECTING: emit selecting(); break;
    case SelectorStatus::CAPTURED:
        update();
        emit captured();
        break;
    case SelectorStatus::LOCKED: emit locked(); break;
    default:                     break;
    }

    emit statusChanged(status_);
}

void Selector::start(const probe::graphics::window_filter_t flags)
{
    if (status_ == SelectorStatus::READY) {
        setMouseTracking(true);

        box_.range(probe::graphics::virtual_screen_geometry());

        hunter::ready(flags);
        geometry_ = probe::graphics::virtual_screen_geometry();
        select(hunter::hunt(QCursor::pos()));
        info_->show();

        status(SelectorStatus::PREY_SELECTING);
    }
}

void Selector::mousePressEvent(QMouseEvent *event)
{
    auto pos = event->globalPosition().toPoint();

    if (event->button() == Qt::LeftButton && status_ != SelectorStatus::LOCKED) {
        cursor_pos_ = box_.absolutePos(pos);

        switch (status_) {
        case SelectorStatus::PREY_SELECTING:
#ifdef _WIN32
            if (scope_ == scope_t::display) {
                auto display = probe::graphics::display_contains(pos);
                if (display.has_value()) {
                    box_.range(display.value().geometry);
                }
            }
#endif

            sbegin_ = pos;
            status(SelectorStatus::PREY_SELECTING | SelectorStatus::FREE_SELECTING);
            break;

        case SelectorStatus::CAPTURED:
            if (cursor_pos_ == ResizerLocation::EMPTY_INSIDE) {
                move_spos_ = move_epos_ = pos;

                status(SelectorStatus::MOVING);
            }
            else if (!!(cursor_pos_ & ResizerLocation::ADJUST_AREA)) {
                status(SelectorStatus::RESIZING);
            }
            break;

        default: loge("error status"); break;
        }
    }
    else if (event->button() == Qt::RightButton) {
        switch (status_) {
        case SelectorStatus::PREY_SELECTING: emit stopped(); break;
        case SelectorStatus::CAPTURED:
            select(hunter::hunt(QCursor::pos()));
            status(SelectorStatus::PREY_SELECTING);
            break;
        default: break;
        }
    }
}

void Selector::mouseMoveEvent(QMouseEvent *event)
{
    auto mouse_pos = event->globalPosition().toPoint();

    switch (status_) {
    case SelectorStatus::PREY_SELECTING:
        select(hunter::hunt(QCursor::pos()));
        setCursor(Qt::CrossCursor);
        break;

    case SelectorStatus::PREY_SELECTING | SelectorStatus::FREE_SELECTING:
        if (std::abs(mouse_pos.x() - sbegin_.x()) >= 4 && std::abs(mouse_pos.y() - sbegin_.y()) >= 4) {
            select({ sbegin_, mouse_pos });
            info_->show();
            status(SelectorStatus::FREE_SELECTING);
        }
        break;

    case SelectorStatus::FREE_SELECTING:
        adjust(0, 0, mouse_pos.x() - box_.x2(), mouse_pos.y() - box_.y2());
        break;

    case SelectorStatus::CAPTURED:
        switch (box_.relativePos(mouse_pos)) {
        case ResizerLocation::EMPTY_INSIDE: setCursor(Qt::SizeAllCursor); break;
        case ResizerLocation::OUTSIDE:      setCursor(Qt::ForbiddenCursor); break;

        case ResizerLocation::T_ANCHOR:
        case ResizerLocation::B_ANCHOR:
        case ResizerLocation::T_BORDER:
        case ResizerLocation::B_BORDER:     setCursor(Qt::SizeVerCursor); break;

        case ResizerLocation::L_ANCHOR:
        case ResizerLocation::R_ANCHOR:
        case ResizerLocation::L_BORDER:
        case ResizerLocation::R_BORDER:     setCursor(Qt::SizeHorCursor); break;

        case ResizerLocation::TL_ANCHOR:
        case ResizerLocation::BR_ANCHOR:    setCursor(Qt::SizeFDiagCursor); break;
        case ResizerLocation::BL_ANCHOR:
        case ResizerLocation::TR_ANCHOR:    setCursor(Qt::SizeBDiagCursor); break;
        default:                            break;
        }
        break;

    case SelectorStatus::MOVING: {
        move_epos_ = mouse_pos;

        translate(move_epos_.x() - move_spos_.x(), move_epos_.y() - move_spos_.y());

        move_spos_ = mouse_pos;

        setCursor(Qt::SizeAllCursor);
        break;
    }

    case SelectorStatus::RESIZING:
        // clang-format off
        switch (cursor_pos_) {
        case ResizerLocation::Y1_BORDER: case ResizerLocation::Y1_ANCHOR: adjust(0, mouse_pos.y() - box_.y1(), 0, 0); break;
        case ResizerLocation::Y2_BORDER: case ResizerLocation::Y2_ANCHOR: adjust(0, 0, 0, mouse_pos.y() - box_.y2()); break;
        case ResizerLocation::X1_BORDER: case ResizerLocation::X1_ANCHOR: adjust(mouse_pos.x() - box_.x1(), 0, 0, 0); break;
        case ResizerLocation::X2_BORDER: case ResizerLocation::X2_ANCHOR: adjust(0, 0, mouse_pos.x() - box_.x2(), 0); break;

        case ResizerLocation::X1Y1_ANCHOR: adjust(mouse_pos.x() - box_.x1(), mouse_pos.y() - box_.y1(), 0, 0); break;
        case ResizerLocation::X1Y2_ANCHOR: adjust(mouse_pos.x() - box_.x1(), 0, 0, mouse_pos.y() - box_.y2()); break;
        case ResizerLocation::X2Y1_ANCHOR: adjust(0, mouse_pos.y() - box_.y1(), mouse_pos.x() - box_.x2(), 0); break;
        case ResizerLocation::X2Y2_ANCHOR: adjust(0, 0, mouse_pos.x() - box_.x2(), mouse_pos.y() - box_.y2()); break;

        default: break;
        }
        // clang-format on
        break;

    case SelectorStatus::LOCKED:
    default:                     break;
    }
}

void Selector::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        switch (status_) {
        case SelectorStatus::PREY_SELECTING:
        case SelectorStatus::PREY_SELECTING | SelectorStatus::FREE_SELECTING:
            status(SelectorStatus::CAPTURED);
            break;
        case SelectorStatus::FREE_SELECTING:
        case SelectorStatus::MOVING:
        case SelectorStatus::RESIZING:
            if (invalid()) {
                select(hunter::hunt(QCursor::pos()));
            }
            status(SelectorStatus::CAPTURED);
            break;
        default: break;
        }
    }
}

void Selector::closeEvent(QCloseEvent *event)
{
    status(SelectorStatus::READY);
    setMouseTracking(false);

    info_->hide();

    box_.coords(box_.range());
    prey_ = {};

    repaint();

    mask_hidden_ = false;
    crosshair_   = false;

    QWidget::closeEvent(event);
}

void Selector::wheelEvent(QWheelEvent *event)
{
    if (status_ == SelectorStatus::PREY_SELECTING) {
        (event->angleDelta().y() > 0) ? select(hunter::contains(prey_))
                                      : select(hunter::contained(prey_, QCursor::pos()));
    }

    QWidget::wheelEvent(event);
}

void Selector::update_info_label()
{
    std::string str{ "-- x --" };

    if (!invalid()) {
        str = fmt::format("{} x {}", selected().width(), selected().height());
        if (prey_.type == hunter::prey_type_t::display) {
            str += " : " + prey_.codename + " - " + prey_.name;
        }
        else if (prey_.type != hunter::prey_type_t::rectangle) {
            str += prey_.name.empty()
                       ? (prey_.codename.empty() ? std::string{} : " : [" + prey_.codename + "]")
                       : " : " + prey_.name;
        }
    }

    info_->setText(QString::fromUtf8(str.c_str()));

    info_->adjustSize();

    // move
    auto info_y = box_.top() - info_->geometry().height() - 1;
    info_->move(QPoint(box_.left() + 1, (info_y < 0 ? box_.top() + 1 : info_y - 1)) - geometry_.topLeft());
}

void Selector::paintEvent(QPaintEvent *)
{
    painter_.begin(this);

    if (coordinate_ != QRect{}) painter_.setWindow(coordinate_);

    const auto srect = selected();

    if (!mask_hidden_) {
        // draw cross-hair
        if (crosshair_ && status_ < SelectorStatus::CAPTURED) {
            painter_.save();

            const auto pos = QCursor::pos();

            painter_.setRenderHint(QPainter::Antialiasing);

            painter_.setPen(QPen{ Qt::white, 1, Qt::DashLine });

            painter_.drawLines(QVector<QLine>{
                { { 0, pos.y() }, { width(), pos.y() } },
                { { pos.x(), 0 }, QPoint{ pos.x(), height() } },
            });

            painter_.setPen(QPen{ Qt::white, 5, Qt::SolidLine, Qt::RoundCap });

            painter_.drawLines(QVector<QLine>{
                { { pos.x() - 75, pos.y() }, { pos.x() - 25, pos.y() } },
                { { pos.x() + 75, pos.y() }, { pos.x() + 25, pos.y() } },

                { { pos.x(), pos.y() - 75 }, QPoint{ pos.x(), pos.y() - 25 } },
                { { pos.x(), pos.y() + 75 }, QPoint{ pos.x(), pos.y() + 25 } },
            });

            painter_.restore();
        }

        // draw mask
        painter_.save();

        painter_.setPen(mask_color_);
        painter_.setBrush(mask_color_);
        painter_.setClipping(true);
        painter_.setClipRegion(QRegion(painter_.window()).subtracted(QRegion(srect)));
        painter_.drawRect(painter_.window());
        painter_.setClipping(false);

        painter_.restore();

        if (status_ > SelectorStatus::READY) {
            // info label
            update_info_label();

            // draw border
            painter_.setPen(pen_);
            painter_.setBrush(QColor(0, 0, 0, 1));
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
        painter_.drawRect(srect.adjusted(-1, -1, 1 + srect.width() % 2, 1 + srect.height() % 2));
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

void Selector::translate(const int32_t dx, const int32_t dy)
{
    if (status_ != SelectorStatus::CAPTURED && status_ != SelectorStatus::MOVING) return;

    box_.translate(dx, dy);
    prey_ = hunter::prey_t::from(box_.rect());

    emit moved();
}

void Selector::adjust(const int32_t dx1, const int32_t dy1, const int32_t dx2, const int32_t dy2)
{
    if (status_ != SelectorStatus::CAPTURED && status_ != SelectorStatus::RESIZING &&
        status_ != SelectorStatus::FREE_SELECTING)
        return;

    box_.adjust(dx1, dy1, dx2, dy2);
    prey_ = hunter::prey_t::from(box_.rect());

    emit resized();
}

void Selector::margins(const int32_t dt, const int32_t dr, const int32_t db, const int32_t dl)
{
    if (status_ != SelectorStatus::CAPTURED && status_ != SelectorStatus::RESIZING) return;

    box_.margins(dt, dr, db, dl);
    prey_ = hunter::prey_t::from(box_.rect());

    emit resized();
}

void Selector::registerShortcuts()
{
    // clang-format off
    // move 1 pixel
    connect(new QShortcut(Qt::Key_W,     this), &QShortcut::activated, [this] { translate(0, -1); });
    connect(new QShortcut(Qt::Key_Up,    this), &QShortcut::activated, [this] { translate(0, -1); });

    connect(new QShortcut(Qt::Key_S,     this), &QShortcut::activated, [this] { translate(0, +1); });
    connect(new QShortcut(Qt::Key_Down,  this), &QShortcut::activated, [this] { translate(0, +1); });

    connect(new QShortcut(Qt::Key_A,     this), &QShortcut::activated, [this] { translate(-1, 0); });
    connect(new QShortcut(Qt::Key_Left,  this), &QShortcut::activated, [this] { translate(-1, 0); });

    connect(new QShortcut(Qt::Key_D,     this), &QShortcut::activated, [this] { translate(+1, 0); });
    connect(new QShortcut(Qt::Key_Right, this), &QShortcut::activated, [this] { translate(+1, 0); });

    // resize
    // expand 1 pixel
    connect(new QShortcut(Qt::CTRL | Qt::Key_Up,    this), &QShortcut::activated, [this] { margins(-1, 0, 0, 0); });
    connect(new QShortcut(Qt::CTRL | Qt::Key_Down,  this), &QShortcut::activated, [this] { margins(0, 0, +1, 0); });
    connect(new QShortcut(Qt::CTRL | Qt::Key_Left,  this), &QShortcut::activated, [this] { margins(0, 0, 0, -1); });
    connect(new QShortcut(Qt::CTRL | Qt::Key_Right, this), &QShortcut::activated, [this] { margins(0, +1, 0, 0); });

    // shrink 1 pixel
    connect(new QShortcut(Qt::SHIFT | Qt::Key_Up,   this), &QShortcut::activated, [this] { margins(+1, 0, 0, 0); });
    connect(new QShortcut(Qt::SHIFT | Qt::Key_Down, this), &QShortcut::activated, [this] { margins(0, 0, -1, 0); });
    connect(new QShortcut(Qt::SHIFT | Qt::Key_Left, this), &QShortcut::activated, [this] { margins(0, 0, 0, +1); });
    connect(new QShortcut(Qt::SHIFT | Qt::Key_Right,this), &QShortcut::activated, [this] { margins(0, -1, 0, 0); });
    // clang-format on

    // fullscreen
    connect(new QShortcut(Qt::CTRL | Qt::Key_A, this), &QShortcut::activated, [this] {
        if (status_ <= SelectorStatus::CAPTURED &&
            ((prey_.type != hunter::prey_type_t::desktop) ||
             (prey_.type != hunter::prey_type_t::display && scope_ == scope_t::display))) {

            status(SelectorStatus::PREY_SELECTING);

            // rectangle / window -> display
            if (prey_.type < hunter::prey_type_t::display) {
                for (const auto& display : probe::graphics::displays()) {
                    if (QRect(display.geometry).contains(box_.rect(), false)) {
                        select(display);
                        status(SelectorStatus::CAPTURED);
                        return;
                    }
                }
            }

            if (prey_.type <= hunter::prey_type_t::display && scope_ == scope_t::desktop) {
                auto desktop = probe::graphics::virtual_screen();
                select(hunter::prey_t{
                    .type     = hunter::prey_type_t::desktop,
                    .geometry = desktop.geometry,
                    .handle   = desktop.handle,
                    .name     = desktop.name,
                    .codename = desktop.classname,
                });
                status(SelectorStatus::CAPTURED);
            }
        }
    });
}

void Selector::setBorderStyle(const QPen& pen) { pen_ = pen; }

void Selector::setMaskStyle(const QColor& c) { mask_color_ = c; }

void Selector::showRegion()
{
    info_->hide();
    mask_hidden_ = true;

    repaint();
}
