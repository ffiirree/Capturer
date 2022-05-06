#include "arroweditmenu.h"
#include "colorpanel.h"

ArrowEditMenu::ArrowEditMenu(QWidget * parent)
    : EditMenu(parent)
{
    auto color_panel = new ColorPanel();
    connect(color_panel, &ColorPanel::changed, [this](const QColor& color) {
        pen_.setColor(color);
        emit changed();
    });
    connect(this, &EditMenu::styleChanged, [=]() { color_panel->setColor(pen().color()); });
    addWidget(color_panel);

    pen_ = QPen(color_panel->color(), 1, Qt::SolidLine);
}
