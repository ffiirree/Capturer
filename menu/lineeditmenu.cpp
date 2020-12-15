#include "lineeditmenu.h"
#include <QFrame>
#include "linewidthwidget.h"
#include "colorpanel.h"

LineEditMenu::LineEditMenu(QWidget * parent)
    : EditMenu(parent)
{
    auto line_width = new WidthButton({HEIGHT, HEIGHT}, 3, true);
    connect(line_width, &WidthButton::changed, [this](int width){
        emit changed();
        pen_.setWidth(width);
    });
    addButton(line_width);

    addSeparator();

    // color button
    auto color_panel = new ColorPanel();
    connect(color_panel, &ColorPanel::changed, [this](const QColor& color){
        pen_.setColor(color);
        emit changed();
    });
    addWidget(color_panel);

    line_width->setChecked(true);
    pen_ = QPen(color_panel->color(), line_width->value(), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    connect(this, &EditMenu::styleChanged, [=]() {
        line_width->setValue(pen().width());
        color_panel->setColor(pen().color());
    });
}
