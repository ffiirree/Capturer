#include "editmenu.h"
#include <QHBoxLayout>

EditMenu::EditMenu(QWidget *parent)
    : QWidget(parent)
{
    setCursor(Qt::ArrowCursor);
    setFixedHeight(HEIGHT);

    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    auto layout = new QHBoxLayout();
    layout->setSpacing(0);
    layout->setMargin(0);
    setLayout(layout);
}

void EditMenu::addWidget(QWidget *widget)
{
    layout()->addWidget(widget);
}
