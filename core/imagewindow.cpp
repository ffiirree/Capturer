#include "imagewindow.h"
#include <QKeyEvent>

ImageWindow::ImageWindow(QWidget *parent)
    : QDialog(parent)
{
    this->move(0, 0);
    this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Dialog);
    this->setCursor(Qt::SizeAllCursor);
    this->setMouseTracking(true);

    label_ = new QLabel();

    layout_ = new QHBoxLayout();
    layout_->setMargin(0);
    layout_->addWidget(label_);
    this->setLayout(layout_);
}

void ImageWindow::fix(QPixmap image)
{
    label_->setPixmap(image);
    this->show();
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
