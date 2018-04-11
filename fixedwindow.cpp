#include "fixedwindow.h"
#include <QKeyEvent>

FixImageWindow::FixImageWindow(QWidget *parent)
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

void FixImageWindow::fix(QPixmap image)
{
    label_->setPixmap(image);
    this->show();
}

void FixImageWindow::mousePressEvent(QMouseEvent *event)
{
    begin_ = event->pos();
}

void FixImageWindow::mouseMoveEvent(QMouseEvent* event)
{
    move(event->pos() - begin_ + pos());
}

void FixImageWindow::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
}

void FixImageWindow::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Escape) {
        hide();
    }
}


FixImageWindow::~FixImageWindow()
{
    delete label_;
    delete layout_;
}
