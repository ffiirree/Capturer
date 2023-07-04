#ifndef _WIN32

#include "framelesswindow.h"

#include <QMouseEvent>

FramelessWindow::FramelessWindow(QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint)
{}

void FramelessWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        moving_begin_ = event->globalPos() - pos();
    }
    return QWidget::mousePressEvent(event);
}

void FramelessWindow::mouseReleaseEvent(QMouseEvent *event) { return QWidget::mouseReleaseEvent(event); }

void FramelessWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    return QWidget::mouseDoubleClickEvent(event);
}

void FramelessWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPos() - moving_begin_);
    }

    return QWidget::mouseMoveEvent(event);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool FramelessWindow::nativeEvent(const QByteArray& eventType, void *message, qintptr *result)
#else
bool FramelessWindow::nativeEvent(const QByteArray& eventType, void *message, long *result)
#endif
{
    return QWidget::nativeEvent(eventType, message, result);
}

#endif
