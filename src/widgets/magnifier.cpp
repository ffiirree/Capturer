#include "magnifier.h"

#include "probe/graphics.h"

#include <QApplication>
#include <QGuiApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QScreen>

Magnifier::Magnifier(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    // pixmap size
    psize_ = msize_ * alpha_;

    // label size
    label_ = new QLabel(this);
    label_->setObjectName("magnifier-color-label");
    label_->setGeometry(0, psize_.height(), psize_.width(), 50);
    label_->setAlignment(Qt::AlignCenter);

    // size
    setFixedSize(psize_.width(), psize_.height() + label_->height());

    // TODO: listen sytem level mouse move event, not just this APP
    qApp->installEventFilter(this);

    hide();
}

QRect Magnifier::mrect()
{
    auto mouse_pos = QCursor::pos();
    return {
        mouse_pos.x() - msize_.width() / 2,
        mouse_pos.y() - msize_.height() / 2,
        msize_.width(),
        msize_.height(),
    };
}

QString Magnifier::getColorStringValue()
{
    return (color_format_ == ColorFormat::HEX) ? center_color_.name(QColor::HexRgb)
                                               : QString("%1, %2, %3")
                                                     .arg(center_color_.red())
                                                     .arg(center_color_.green())
                                                     .arg(center_color_.blue());
}

QPoint Magnifier::position()
{
    auto cx = QCursor::pos().x(), cy = QCursor::pos().y();
    auto rg = probe::graphics::virtual_screen_geometry();

    int mx = (rg.right() - cx > width()) ? cx + 16 : cx - width() - 16;
    int my = (rg.bottom() - cy > height()) ? cy + 16 : cy - height() - 16;
    return { mx, my };
}

bool Magnifier::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseMove) {
        update();
        move(position());
    }
    else {
        return qApp->eventFilter(obj, event);
    }
}

// TODO: clear the background when close
void Magnifier::showEvent(QShowEvent *) { move(position()); }

void Magnifier::paintEvent(QPaintEvent *)
{
    auto area   = mrect();
    auto pixmap = QGuiApplication::primaryScreen()->grabWindow(
        probe::graphics::virtual_screen().handle, area.x(), area.y(), area.width(), area.height());

    QPainter painter(this);

    // 0.
    auto draw_ = pixmap.scaled(psize_, Qt::KeepAspectRatioByExpanding);

    // 1.
    painter.fillRect(rect(), QColor(0, 0, 0, 150));
    painter.drawPixmap(0, 0, draw_);

    // 2.
    painter.setPen(QPen(QColor(0, 100, 250, 125), 5, Qt::SolidLine, Qt::FlatCap));

    painter.drawLine(QPoint(0, psize_.height() / 2), QPoint(psize_.width(), psize_.height() / 2));
    painter.drawLine(QPoint(psize_.width() / 2, 0), QPoint(psize_.width() / 2, psize_.height()));

    // 3.
    center_color_ = QColor(draw_.toImage().pixel(psize_.width() / 2, psize_.height() / 2));
    painter.setPen(QPen(center_color_, 5, Qt::SolidLine, Qt::FlatCap));
    painter.drawLine(QPoint(psize_.width() / 2 - 2, psize_.height() / 2),
                     QPoint(psize_.width() / 2 + 3, psize_.height() / 2));

    // 4. border
    painter.setPen(QPen(Qt::black));
    painter.drawRect(0, 0, psize_.width() - 1, psize_.height() - 1);
    painter.setPen(QPen(Qt::white));
    painter.drawRect(1, 1, psize_.width() - 3, psize_.height() - 3);
    painter.end();

    // 5. text
    auto text = QString("POS : %1, %2\nRGB : ").arg(QCursor::pos().x()).arg(QCursor::pos().y()) +
                getColorStringValue();
    label_->setText(text);
}