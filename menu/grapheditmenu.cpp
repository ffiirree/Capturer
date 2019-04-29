#include "grapheditmenu.h"
#include <QStyle>
#include <QHBoxLayout>
#include "colorpanel.h"
#include "linewidthwidget.h"
#include "separator.h"
#include "iconbutton.h"

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
    addWidget(width_btn);
    group->addButton(width_btn);

    addWidget(new Separator());

    auto fill_btn = new IconButton(QPixmap(":/icon/res/fill"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W }, true);
    connect(fill_btn, &IconButton::toggled, [=](bool checked) {
        fill_ = checked;
        pen_.setWidth(fill_ ? 0 : width_btn->value());

        emit changed();
    });
    addWidget(fill_btn);
    group->addButton(fill_btn);

    addWidget(new Separator());

    // color button
    auto color_panel = new ColorPanel();
    connect(color_panel, &ColorPanel::changed, [this](const QColor& c){
        emit changed();
        pen_.setColor(c);
    });
    addWidget(color_panel);

    width_btn->setChecked(true);
    pen_ = QPen(color_panel->color(), width_btn->value(), Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
}
