#include "editmenu.h"
#include <QHBoxLayout>
#include "separator.h"
#include "config.h"

EditMenu::EditMenu(QWidget *parent)
    : QWidget(parent)
{
    setCursor(Qt::ArrowCursor);
    setFixedHeight(HEIGHT);

    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowFlags(windowFlags() | Qt::ToolTip | Qt::WindowStaysOnTopHint);

    auto layout = new QHBoxLayout();
    layout->setSpacing(0);
    layout->setMargin(0);
    setLayout(layout);
}

void EditMenu::addButton(CustomButton * btn)
{
    if (Config::instance()["theme"].get<QString>() == "dark") {
        btn->normal(QColor("#e6e6e6"), QColor("#232323"));
        btn->hover(QColor("#e6e6e6"), QColor("#323232"));
        btn->checked(Qt::white, QColor("#409eff"));
    }
    else {
        btn->normal(QColor("#2c2c2c"), QColor("#f9f9f9"));
        btn->hover(QColor("#2c2c2c"), QColor("#dfdfdf"));
        btn->checked(Qt::white, QColor("#409eff"));
    }

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
