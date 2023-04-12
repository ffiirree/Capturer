#include "widthbutton.h"
#include <QWheelEvent>
#include "fmt/format.h"

void WidthButton::setValue(int width)
{
    width_ = std::clamp(width, min_, max_);

    const double max_em = 2.5;
    double width_em = ((double)width_ / max_) * 1.75;

    std::string style = fmt::format("WidthButton::indicator{{ height: {:2.1f}em; width: {:2.1f}em; margin: {:4.3f}em; border-radius: {:4.3f}em; }}",
                                    width_em, width_em, (static_cast<int>((max_em - width_em) * 49.9)) / 100.0, width_em * 0.485);

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
