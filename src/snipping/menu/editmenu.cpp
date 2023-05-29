#include "editmenu.h"

#include "separator.h"

#include <QHBoxLayout>

EditMenu::EditMenu(QWidget *parent)
    : QWidget(parent)
{
    setCursor(Qt::ArrowCursor);

    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowFlags(windowFlags() | Qt::ToolTip | Qt::WindowStaysOnTopHint);

    setLayout(new QHBoxLayout(this));
    layout()->setSpacing(0);
    layout()->setContentsMargins({});
}

void EditMenu::addSeparator() { layout()->addWidget(new Separator()); }

void EditMenu::addWidget(QWidget *widget) { layout()->addWidget(widget); }

void EditMenu::moveEvent(QMoveEvent *) { emit moved(); }
