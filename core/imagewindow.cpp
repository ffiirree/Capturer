#include "imagewindow.h"
#include <QKeyEvent>
#include <QMainWindow>
#include <QPainter>
#include <QGraphicsDropShadowEffect>

ImageWindow::ImageWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Dialog);
    setCursor(Qt::SizeAllCursor);

    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);

    auto effect = new QGraphicsDropShadowEffect(this);
    effect->setBlurRadius(10);
    effect->setOffset(0);
    effect->setColor(QColor(0, 125, 255));
    setGraphicsEffect(effect);
}

void ImageWindow::fix(QPixmap image)
{
    pixmap_ = image;
    size_ = pixmap_.size();

    resize(size_ + QSize{10, 10});

    update();
    show();
}

void ImageWindow::mousePressEvent(QMouseEvent *event)
{
    begin_ = event->pos();
}

void ImageWindow::mouseMoveEvent(QMouseEvent* event)
{
    move(event->pos() - begin_ + pos());
}

void ImageWindow::wheelEvent(QWheelEvent *event)
{
    scale_ += (event->delta()/12000.0);         // +/-1%
    scale_ = scale_ < 0.1 ? 0.1 : scale_;

    center_ = geometry().center();
    auto _size = (size_) * scale_ + QSize{10, 10};
    setGeometry(center_.x() - _size.width()/2, center_.y() - _size.height()/2, _size.width(), _size.height());

    update();
}

void ImageWindow::paintEvent(QPaintEvent *)
{
    QPainter p{this};
    auto pixmap = pixmap_.scaled(size_ * scale_, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    p.drawPixmap(5, 5, pixmap);
}

void ImageWindow::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Escape) {
        close();
    }
}
