#include "buttongroup.h"

CustomButton::CustomButton(QWidget *parent)
    : QAbstractButton (parent)
{
    connect(this, &CustomButton::toggled, [this](bool checked){
        background_ = checked ? checked_color_ : normal_color_;
        icon_color_ = checked ? Qt::white : Qt::black;
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
    background_ = isChecked() ? background_ : hover_color_;
}

void CustomButton::leaveEvent(QEvent *)
{
    background_ = isChecked() ? background_ : normal_color_;
}

void CustomButton::paintEvent(QPaintEvent *)
{
    painter_.begin(this);
    painter_.fillRect(rect(), background_);

    paint(&painter_);

    if(!isEnabled())
        painter_.fillRect(rect(), QColor{ 200, 200, 200, 150});
    painter_.end();
}


void ButtonGroup::addButton(QAbstractButton *btn)
{
    if(!btn) return;

    buttons_.append(btn);
    connect(btn, &QAbstractButton::clicked, [=](){ emit buttonClicked(btn); });
    connect(btn, &QAbstractButton::toggled, [=](bool checked) {
        emit buttonToggled(btn, checked);

        if(checked) {
            auto last_checked = checked_;
            checked_ = btn;
            // Set last checked button unchecked
            if(last_checked) {
                last_checked->setChecked(false);
            }
        }
        else {
            // The checked button set itself unchecked => none
            if(checked_ == btn) {
                checked_ = nullptr;
                emit uncheckedAll();
            }
        }
    });
}

void ButtonGroup::uncheckAll()
{
    if(checked_) {
        checked_->setChecked(false);
        checked_ = nullptr;
        emit uncheckedAll();
    }
}
