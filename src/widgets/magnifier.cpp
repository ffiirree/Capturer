#include "magnifier.h"
#include <QPainter>
#include <QApplication>
#include "probe/graphics.h"

Magnifier::Magnifier(QWidget *parent)
    :QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    // pixmap size
    psize_ = msize_ * alpha_;

    // label size
    label_ = new QLabel(this);
    label_->setObjectName("magnifier-color-label");
    label_->setGeometry(0, psize_.height(), psize_.width(), 30);
    label_->setAlignment(Qt::AlignCenter);

    // size
    setFixedSize(psize_.width(), psize_.height() + label_->height());

    hide();
}

QRect Magnifier::mrect()
{
    auto mouse_pos = QCursor::pos() - QRect(probe::graphics::virtual_screen_geometry()).topLeft();
    return { mouse_pos.x() - msize_.width()/2, mouse_pos.y() - msize_.height()/2, msize_.width(), msize_.height() };
}

void Magnifier::paintEvent(QPaintEvent * e)
{
    Q_UNUSED(e);

    QPainter painter(this);

    // 0.
    auto draw_ = pixmap_.scaled(psize_, Qt::KeepAspectRatioByExpanding);

    // 1.
    painter.fillRect(rect(), QColor(0, 0, 0, 150));
    painter.drawPixmap(0, 0, draw_);

    // 2.
    painter.setPen(QPen(QColor(0, 100, 250, 125), 5, Qt::SolidLine, Qt::FlatCap));

    painter.drawLine(QPoint(0,                  psize_.height()/2),    QPoint(psize_.width(),  psize_.height()/2));
    painter.drawLine(QPoint(psize_.width()/2,   0),                    QPoint(psize_.width()/2,  psize_.height()));

    // 3.
    center_color_ = QColor(draw_.toImage().pixel(psize_.width()/2, psize_.height()/2));
    painter.setPen(QPen(center_color_, 5, Qt::SolidLine, Qt::FlatCap));
    painter.drawLine(QPoint(psize_.width()/2 - 2, psize_.height()/2),  QPoint(psize_.width()/2 + 3, psize_.height()/2));

    // 4. border
    painter.setPen(QPen(Qt::black));
    painter.drawRect(0, 0, psize_.width() - 1, psize_.height() - 1);
    painter.setPen(QPen(Qt::white));
    painter.drawRect(1, 1, psize_.width() - 3, psize_.height() - 3);
    painter.end();

    // 5. text
    auto text = getColorStringValue();
    label_->setText(text);
}
