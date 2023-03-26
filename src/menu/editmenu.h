#ifndef EDIT_MENU_H
#define EDIT_MENU_H

#include <QWidget>
#include <QPen>
#include "iconbutton.h"

class EditMenu : public QWidget
{
    Q_OBJECT

public:
    explicit EditMenu(QWidget *parent = nullptr);

    virtual QColor color() { return color_; }
    virtual void color(const QColor& c) { color_ = c; }

    virtual int lineWidth() { return width_; }
    virtual void lineWidth(int w) { width_ = std::max<int>(1, w); }

    virtual bool fill() { return fill_; }
    virtual void fill(bool fill) { fill_ = fill; }

    virtual QFont font() { return font_; }
    virtual void font(const QFont& font) { font_ = font;  }

signals:
    void changed();             // out event
    void moved();

protected:
    void moveEvent(QMoveEvent*) override;

    void addButton(CustomButton*);
    void addSeparator();
    void addWidget(QWidget *);

    QColor color_{ Qt::red };
    int width_{ 3 };
    bool fill_{ false };
    QFont font_;                // family, style, size
};

#endif // EDIT_MENU_H
