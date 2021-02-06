#ifndef ERASE_MENU_H
#define ERASE_MENU_H

#include "editmenu.h"
#include "linewidthwidget.h"

class EraseMenu : public EditMenu
{
    Q_OBJECT
public:
    explicit EraseMenu(QWidget *parent = nullptr);

    QPen pen() const  override { return pen_; }
    void pen(QPen pen) override
    {
        pen_ = pen;
        width_->setValue(pen.width());
    }

private:
    WidthButton *width_ = nullptr;
};

#endif // ERASE_MENU_H
