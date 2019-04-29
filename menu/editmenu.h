#ifndef EDITMENU_H
#define EDITMENU_H

#include <QWidget>
#include <QPen>

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
    void addWidget(QWidget *);

    const int HEIGHT = 34;
    const int ICON_W = 24;

    QPen pen_;
    bool fill_ = false;
};

#endif // EDITMENU_H
