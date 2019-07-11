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
    setFixedHeight(HEIGHT + 2);

    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowFlags(Qt::ToolTip | Qt::WindowStaysOnTopHint);

    auto layout = new QHBoxLayout();
    layout->setSpacing(0);
    layout->setMargin(1);
    setLayout(layout);
}

void EditMenu::addButton(CustomButton * btn)
{
    btn->setIconCheckedColor(QColor("#ffffff"));
    btn->setBackgroundColor(QColor("#ffffff"));
    btn->setBackgroundHoverColor(QColor("#d0d0d5"));
    btn->setBackgroundCheckedColor(QColor("#2080F0"));

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
