#include "imagewindow.h"
#include <QKeyEvent>
#include <QGraphicsDropShadowEffect>

ImageWindow::ImageWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setCursor(Qt::SizeAllCursor);

    label_ = new QLabel();

    layout_ = new QHBoxLayout();
    layout_->setMargin(0);
    layout_->addWidget(label_);
    setLayout(layout_);

    layout_->setMargin(10);

    setAttribute(Qt::WA_TranslucentBackground);
    auto effect = new QGraphicsDropShadowEffect(this);
    effect->setBlurRadius(10);
    effect->setOffset(0);
    effect->setColor(QColor(0, 125, 255));
    setGraphicsEffect(effect);
}

void ImageWindow::fix(QPixmap image)
{
    label_->setPixmap(image);
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

void ImageWindow::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
}

void ImageWindow::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Escape) {
        hide();
    }
}

ImageWindow::~ImageWindow()
{
    delete label_;
    delete layout_;
}
