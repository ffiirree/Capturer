#include "widthbutton.h"
#include <QWheelEvent>
#include "fmt/format.h"

void WidthButton::setValue(int width)
{
    width_ = std::clamp(width, min_, max_);

    const double max_em = 2.5;
    double width_em = ((double)width_ / max_) * max_em;

    std::string style = fmt::format("WidthButton::indicator{{ height: {}em; width: {}em; margin: {}em; border-radius: {}em; }}",
                                    width_em, width_em, (max_em - width_em) * 4.95, width_em * 0.45);

    setStyleSheet(QString::fromStdString(style));

    update();
}

void WidthButton::wheelEvent(QWheelEvent* event)
{
    if (!isChecked()) return;

    width_ += event->angleDelta().y() / 120;

    setValue(width_);

    emit changed(width_);
}
