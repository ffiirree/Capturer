#ifndef CAPTURER_STYLE_MENU_H
#define CAPTURER_STYLE_MENU_H

#include <QWidget>
#include <QPen>
#include <QFont>

class QCheckBox;
class ComboBox;
class ColorPanel;
class WidthButton;

class StyleMenu : public QWidget
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

    const int HEIGHT = 35;
    const int ICON_W = 21;

    QPen pen_{ Qt::red, 3 };
    QBrush brush_{ Qt::NoBrush };
    QFont font_{};
    bool fill_{};
};

#endif // !CAPTURER_STYLE_MENU_H
