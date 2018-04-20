#include "magnifier.h"
#include <QPainter>
#include <QApplication>
#include <QScreen>
#include <QDesktopWidget>

Magnifier::Magnifier(QWidget *parent)
    :QFrame(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setGeometry(0, 0, 105, 105);

    label_ = new QLabel(this);
    label_->setFixedSize(105, 105);
    label_->setScaledContents(true);
}

void Magnifier::paintEvent(QPaintEvent * e)
{
    Q_UNUSED(e);

    QPainter painter;

    auto draw_ = area_.scaled(105, 105, Qt::KeepAspectRatioByExpanding);
    painter.begin(&draw_);
    painter.setPen(QPen(QColor(50, 50, 200, 100), 5, Qt::SolidLine, Qt::FlatCap));

    painter.drawLine(QPoint(0, 53), QPoint(50, 53));
    painter.drawLine(QPoint(56, 53), QPoint(105, 53));
    painter.drawLine(QPoint(53, 0), QPoint(53, 50));
    painter.drawLine(QPoint(53, 56), QPoint(53, 105));
    painter.end();

    label_->setPixmap(draw_);
}
