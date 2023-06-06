#ifndef CAPTURER_EDITING_SUBMENU_H
#define CAPTURER_EDITING_SUBMENU_H

#include "colorpanel.h"
#include "combobox.h"
#include "widthbutton.h"

#include <QCheckBox>
#include <QFont>
#include <QPen>
#include <QWidget>

class EditingSubmenu : public QWidget
{
    Q_OBJECT
public:
    enum
    {
        WIDTH_BTN   = 0x01,
        FILL_BTN    = 0x02,
        COLOR_PENAL = 0x04,
        FONT_BTNS   = 0x08
    };

public:
    explicit EditingSubmenu(int buttons, QWidget *parent = nullptr);

    //
    void setPen(const QPen& pen);

    QPen pen() const { return pen_; }

    //
    void setBrush(const QBrush& brush);

    QBrush brush() const { return brush_; }

    //
    void setFont(const QFont& font);

    QFont font() const { return font_; }

    //
    bool filled() const;

    void fill(bool);

signals:
    void penChanged(const QPen&);
    void brushChanged(const QBrush&);
    void fontChanged(const QFont&);
    void fillChanged(bool);

    void moved();

private:
    WidthButton *width_btn_{ nullptr };
    QCheckBox *fill_btn_{ nullptr };
    ComboBox *font_family_{ nullptr };
    ComboBox *font_size_{ nullptr };
    ComboBox *font_style_{ nullptr };
    ColorPanel *color_panel_{ nullptr };

    QPen pen_{ Qt::red, 3 };
    QBrush brush_{ Qt::NoBrush };
    QFont font_{};
    bool fill_{};
};

#endif // !CAPTURER_STYLE_MENU_H
