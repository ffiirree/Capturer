#ifndef CAPTURER_STYLE_MENU_H
#define CAPTURER_STYLE_MENU_H

#include "editmenu.h"

class QCheckBox;
class ComboBox;
class ColorPanel;
class WidthButton;

class StyleMenu : public EditMenu
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
    explicit StyleMenu(int buttons, QWidget *parent = nullptr);

    void pen(const QPen& pen) override;

    void brush(const QBrush& brush);

    void font(const QFont& font) override;

    bool fill() const override;

    void fill(bool) override;

private:
    WidthButton *width_btn_{ nullptr };
    QCheckBox *fill_btn_{ nullptr };
    ComboBox *font_family_{ nullptr };
    ComboBox *font_size_{ nullptr };
    ComboBox *font_style_{ nullptr };
    ColorPanel *color_panel_{ nullptr };

    const int HEIGHT = 35;
    const int ICON_W = 21;
};

#endif // !CAPTURER_STYLE_MENU_H
