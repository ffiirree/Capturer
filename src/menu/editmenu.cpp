#include "editmenu.h"
#include <QHBoxLayout>
#include "separator.h"

EditMenu::EditMenu(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("EditMenu");
    setAttribute(Qt::WA_StyledBackground);
    setAutoFillBackground(true);

    setCursor(Qt::ArrowCursor);
    setFixedHeight(HEIGHT);

    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowFlags(Qt::ToolTip | Qt::WindowStaysOnTopHint);

    auto layout = new QHBoxLayout();
    layout->setSpacing(0);
    layout->setMargin(0);
    setLayout(layout);
}

void EditMenu::addButton(CustomButton * btn)
{
    btn->setIconCheckedColor(QColor("#ffffff"));
    btn->setBackgroundHoverColor(QColor("#d0d0d5"));
    btn->setBackgroundCheckedColor(QColor("#409eff"));

    layout()->addWidget(btn);
}

void EditMenu::addSeparator()
{
    layout()->addWidget(new Separator());
}

void EditMenu::addWidget(QWidget * widget)
{
    layout()->addWidget(widget);
}

void EditMenu::moveEvent(QMoveEvent* event)
{
    Q_UNUSED(event);
    emit moved();
}
