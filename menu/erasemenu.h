#ifndef ERASEMENU_H
#define ERASEMENU_H

#include "editmenu.h"
#include "linewidthwidget.h"

class EraseMenu : public EditMenu
{
    Q_OBJECT
public:
    explicit EraseMenu(QWidget *parent = nullptr);

    QPen pen() const  override { return pen_; }
    void pen(int width) override
    {
        pen_.setWidth(width);
        width_->setValue(width);
    }

private:
    WidthButton *width_ = nullptr;
};


#endif // ERASEMENU_H
