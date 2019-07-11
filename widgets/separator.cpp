#include "separator.h"

Separator::Separator(QWidget *parent)
    : QFrame(parent)
{
    setFixedHeight(20);
    setFrameStyle(QFrame::Raised);
    setFrameShape(QFrame::VLine);
}

Separator::Separator(Shape shape, int len, QWidget *parent)
    : QFrame(parent)
{
    setFrameShape(shape);
    switch (shape) {
    case QFrame::VLine: setFixedHeight(len); break;
    case QFrame::HLine: setFixedWidth(len); break;
    default: break;
    }

    setFrameStyle(QFrame::Raised);
}
