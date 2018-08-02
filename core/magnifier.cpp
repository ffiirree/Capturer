#include "magnifier.h"
#include <QPainter>
#include <QApplication>
#include <QScreen>
#include <QDesktopWidget>
#include <QDebug>

Magnifier::Magnifier(QWidget *parent)
    :QFrame(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    // pixmap size
    psize_ = msize_ * alpha_;

    // label size
    label_ = new QLabel(this);
    label_->setGeometry(0, psize_.height(), psize_.width(), 30);
    label_->setAlignment(Qt::AlignCenter);

    QFont font;
    font.setPointSize(9);
    font.setFamily("Consolas");
    label_->setFont(font);

    QPalette p;
    p.setColor(QPalette::WindowText, Qt::white);
    label_->setPalette(p);

    // size
    setFixedSize(psize_.width(), psize_.height() + label_->height());

    hide();
}

QRect Magnifier::mrect()
{
    auto mouse_pos = QCursor::pos();
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
    painter.setPen(QPen(QColor(0, 100, 250, 200), 5, Qt::SolidLine, Qt::FlatCap));

    painter.drawLine(QPoint(0,                  psize_.height()/2),    QPoint(psize_.width(),  psize_.height()/2));
    painter.drawLine(QPoint(psize_.width()/2,   0),                    QPoint(psize_.width()/2,  psize_.height()));

    // 3.
    auto color = QColor(draw_.toImage().pixel(psize_.width()/2, psize_.height()/2));
    painter.setPen(QPen(color, 5, Qt::SolidLine, Qt::FlatCap));
    painter.drawLine(QPoint(psize_.width()/2 - 2, psize_.height()/2),  QPoint(psize_.width()/2 + 3, psize_.height()/2));

    // 4. border
    painter.setPen(QPen(Qt::white));
    painter.drawRect(0, 0, psize_.width() - 1, psize_.height() - 1);
    painter.end();

    // 5. text
    auto text = QString("RGB:(%1, %2, %3)").arg(color.red()).arg(color.green()).arg(color.blue());
    label_->setText(text);
}
