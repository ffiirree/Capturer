#include "framelesswindow.h"
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QGraphicsDropShadowEffect>
#include "logging.h"

FramelessWindow::FramelessWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags( Qt::Window | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_TranslucentBackground);

    setLayout(new QVBoxLayout(this));
    layout()->setContentsMargins({ 15, 15, 15, 15 });

    effect_ = new QGraphicsDropShadowEffect(this);
    effect_->setBlurRadius(15);
    effect_->setOffset(0, 0);
    effect_->setColor(palette().color(QPalette::Highlight));
    setGraphicsEffect(effect_);

    setMouseTracking(true);
}

void FramelessWindow::setWidget(QWidget* w)
{
    w->setMouseTracking(true);
    layout()->addWidget(w);
}

void FramelessWindow::mousePressEvent(QMouseEvent* e)
{
    if (hover_flags_ != 0) {
        is_resizing_ = true;
        is_moving_ = false;
        resizing_begin_ = e->globalPos();
    }
    else {
        is_resizing_ = false;
        is_moving_ = true;
        moving_begin_ = e->globalPos();
        setCursor(Qt::SizeAllCursor);
    }
}

void FramelessWindow::mouseMoveEvent(QMouseEvent* e)
{
    LOG(INFO) << "mouse moving";

    int x = e->globalPos().x(), y = e->globalPos().y();
    auto r = geometry();

    if (!is_resizing_ && !is_moving_) {
        hover_flags_ = 0x00;

        if (x < r.left() + 15) {
            hover_flags_ |= Qt::LeftEdge;
        }

        if (x > r.right() - 15) {
            hover_flags_ |= Qt::RightEdge;
        }

        if (y < r.top() + 15) {
            hover_flags_ |= Qt::TopEdge;
        }

        if (y > r.bottom() - 15) {
            hover_flags_ |= Qt::BottomEdge;
        }

        switch (hover_flags_) {
        case Qt::LeftEdge:
        case Qt::RightEdge:
            setCursor(Qt::SizeHorCursor);
            break;

        case Qt::TopEdge:
        case Qt::BottomEdge:
            setCursor(Qt::SizeVerCursor);
            break;

        case Qt::LeftEdge | Qt::TopEdge:
        case Qt::RightEdge | Qt::BottomEdge: 
            setCursor(Qt::SizeFDiagCursor);
            break;

        case Qt::LeftEdge | Qt::BottomEdge:
        case Qt::RightEdge | Qt::TopEdge:
            setCursor(Qt::SizeBDiagCursor); 
            break;

        default:
            setCursor(Qt::ArrowCursor); 
            break;
        }
    }

    if (is_moving_) {
        move(geometry().topLeft() + e->globalPos() - moving_begin_);
        moving_begin_ = e->globalPos();
    }

    if (is_resizing_) {
        auto diff = e->globalPos() - resizing_begin_;
        auto geo = geometry();
        resizing_begin_ = e->globalPos();

        if (hover_flags_ & Qt::LeftEdge) {
            geo.adjust(diff.x(), 0, 0, 0);
        }
        else if (hover_flags_ & Qt::RightEdge) {
            geo.adjust(0, 0, diff.x(), 0);
        }

        if (hover_flags_ & Qt::TopEdge) {
            geo.adjust(0, diff.y(), 0, 0);
        }
        else if (hover_flags_ & Qt::BottomEdge) {
            geo.adjust(0, 0, 0, diff.y());
        }

        setGeometry(geo);
    }
}

void FramelessWindow::mouseReleaseEvent(QMouseEvent*)
{
    if (is_moving_ || is_resizing_) {
        is_moving_ = false;
        is_resizing_ = false;
        setCursor(Qt::ArrowCursor);
        moving_begin_ = { -1, -1 };
        resizing_begin_ = { -1, -1 };
    }
}