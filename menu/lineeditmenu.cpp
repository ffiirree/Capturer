#include "lineeditmenu.h"
#include <QFrame>
#include "linewidthwidget.h"
#include "colorpanel.h"
#include "separator.h"

LineEditMenu::LineEditMenu(QWidget * parent)
    : EditMenu(parent)
{
    auto line_width = new WidthButton({HEIGHT, HEIGHT}, 3, true);
    connect(line_width, &WidthButton::changed, [this](int width){
        emit changed();
        pen_.setWidth(width);
    });
    addWidget(line_width);

    addWidget(new Separator());

    // color button
    auto color_panel = new ColorPanel();
    connect(color_panel, &ColorPanel::changed, [this](const QColor& color){
        emit changed();
        pen_.setColor(color);
    });
    addWidget(color_panel);

    line_width->setChecked(true);
    pen_ = QPen(color_panel->color(), line_width->value(), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
}
