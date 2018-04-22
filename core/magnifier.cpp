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
    // 31 * 31
    setFixedSize(w_ * 5, h_ * 5 + 30);

    label_ = new QLabel(this);
    label_->setGeometry(0, h_ * 5, w_ * 5, 30);
    label_->setAlignment(Qt::AlignCenter);

    QFont font;
    font.setPointSize(9);
    font.setFamily("Consolas");
    label_->setFont(font);

    QPalette p;
    p.setColor(QPalette::WindowText, Qt::white);
    label_->setPalette(p);

    hide();
}

QRect Magnifier::area()
{
    auto mouse_pos = QCursor::pos();
    return QRect(mouse_pos.x() - w_/2, mouse_pos.y() - h_/2, w_, h_);
}

void Magnifier::paintEvent(QPaintEvent * e)
{
    Q_UNUSED(e);

    QPainter painter;

    auto draw_ = area_.scaled(w_ * 5, h_ * 5, Qt::KeepAspectRatioByExpanding);
    painter.begin(this);
    // 1.
    painter.fillRect(rect(), QColor(0, 0, 0, 150));
    painter.drawPixmap(0, 0, draw_);

    // 2.
    painter.setPen(QPen(QColor(0, 100, 250, 200), 5, Qt::SolidLine, Qt::FlatCap));

    painter.drawLine(QPoint(0,              h_/2 * 5),    QPoint((w_/2 - 1) * 5 + 3,  h_/2 * 5));
    painter.drawLine(QPoint((w_/2 + 1) * 5 - 2, h_/2 * 5),    QPoint(w_ * 5,    h_/2 * 5));
    painter.drawLine(QPoint(w_/2 * 5, 0),                 QPoint(w_/2 * 5,  (h_/2 - 1)* 5 + 3));
    painter.drawLine(QPoint(w_/2 * 5, (h_/2 + 1) * 5 - 2),    QPoint(w_/2 * 5,  h_ * 5));

    painter.setPen(QPen(Qt::white));
    painter.drawRect(0, 0, w_ * 5 - 1, h_ * 5 - 1);
    painter.end();

    // 3.
    auto color = QColor(draw_.toImage().pixel((w_/2) * 5 + 1, (h_/2) * 5 + 1));
    auto text = "RGB:(" + QString::number(color.red()) + ", " + QString::number(color.red()) + ", " + QString::number(color.red()) + ")";
    label_->setText(text);
}
