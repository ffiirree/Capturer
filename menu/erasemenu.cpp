#include "erasemenu.h"
#include <QPushButton>
#include "iconbutton.h"

EraseMenu::EraseMenu(QWidget *parent)
    : EditMenu(parent)
{

    width_ = new WidthButton({HEIGHT, HEIGHT}, 15, true);
    connect(width_, &WidthButton::changed, [=](int w){
        pen_.setWidth(w);

        emit changed();
    });
    addWidget(width_);

    // after added to the group
    width_->setChecked(true);
    pen_ = QPen(Qt::transparent, width_->value(), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
}
