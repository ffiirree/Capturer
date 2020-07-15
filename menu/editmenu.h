#ifndef EDITMENU_H
#define EDITMENU_H

#include <QWidget>
#include <QPen>
#include "iconbutton.h"

class EditMenu : public QWidget
{
    Q_OBJECT
public:
    explicit EditMenu(QWidget *parent = nullptr);

    virtual QPen pen() const { return pen_; }
    virtual void pen(int w) { pen_.setWidth(w); }

    virtual bool fill() const { return fill_; }

signals:
    void changed();

protected:
    void addButton(CustomButton *);
    void addSeparator();
    void addWidget(QWidget *);

    const int HEIGHT = 35;
    const int ICON_W = 25;

    QPen pen_;
    bool fill_ = false;
};

#endif // EDITMENU_H
