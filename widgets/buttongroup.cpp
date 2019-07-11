#include "buttongroup.h"

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
