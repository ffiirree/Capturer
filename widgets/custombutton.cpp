#include "custombutton.h"

CustomButton::CustomButton(QWidget *parent)
    : QAbstractButton (parent)
{
    connect(this, &CustomButton::toggled, [this](bool checked){
        bg_color_ = checked ? bg_checked_color_ : bg_normal_color_;
        icon_color_ = checked ? icon_checked_color_ : icon_normal_color_;
        update();
    });
}

CustomButton::CustomButton(const QSize& size, bool checkable, QWidget *parent)
    : CustomButton(parent)
{
    setFixedSize(size);
    setCheckable(checkable);
}

void CustomButton::enterEvent(QEvent *)
{
    bg_color_ = isChecked() ? bg_checked_color_ : bg_hover_color_;
    icon_color_ = isChecked() ? icon_checked_color_ : icon_hover_color_;
}

void CustomButton::leaveEvent(QEvent *)
{
    bg_color_ = isChecked() ? bg_checked_color_ : bg_normal_color_;
    icon_color_ = isChecked() ? icon_checked_color_ : icon_normal_color_;
}

void CustomButton::paintEvent(QPaintEvent *)
{
    painter_.begin(this);
    painter_.fillRect(rect(), bg_color_);

    paint(&painter_);

    if(!isEnabled())
        painter_.fillRect(rect(), QColor{ 200, 200, 200, 150});
    painter_.end();
}

