#include "grapheditmenu.h"
#include <QStyle>
#include <QHBoxLayout>
#include "colorpanel.h"
#include "linewidthwidget.h"
#include "separator.h"
#include "iconbutton.h"
#include "buttongroup.h"

GraphMenu::GraphMenu(QWidget * parent)
    : EditMenu(parent)
{
    auto group = new ButtonGroup(this);

    auto width_btn = new WidthButton({ HEIGHT, HEIGHT }, 3, true);
    connect(width_btn, &WidthButton::changed, [=](int w){
        pen_.setWidth(w);
        fill_ = false;

        emit changed();
    });
    addButton(width_btn);
    group->addButton(width_btn);

    addSeparator();

    auto fill_btn = new IconButton(QPixmap(":/icon/res/fill"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W }, true);
    connect(fill_btn, &IconButton::toggled, [=](bool checked) {
        fill_ = checked;
        pen_.setWidth(fill_ ? 0 : width_btn->value());

        emit changed();
    });
    addButton(fill_btn);
    group->addButton(fill_btn);

    addSeparator();

    // color button
    auto color_panel = new ColorPanel();
    connect(color_panel, &ColorPanel::changed, [this](const QColor& c){
        pen_.setColor(c);
        emit changed();
    });
    addWidget(color_panel);

    width_btn->setChecked(true);
    pen_ = QPen(color_panel->color(), width_btn->value(), Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);

    connect(this, &EditMenu::styleChanged, [=]() {
        if(!fill_) width_btn->setValue(pen().width());

        fill_ ? fill_btn->setChecked(true) : width_btn->setChecked(true);
        color_panel->setColor(pen().color());
    });
}
