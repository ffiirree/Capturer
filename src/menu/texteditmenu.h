#ifndef FONT_MENU_H
#define FONT_MENU_H

#include <QComboBox>
#include "editmenu.h"
#include "colorpanel.h"

class FontMenu : public EditMenu
{
    Q_OBJECT

public:
    explicit FontMenu(QWidget* parent = nullptr);

    inline QString  fontFamily()    const { return font_family_->currentText(); }
    inline int      fontSize()      const { return font_size_->currentText().toInt(); }
    inline QString  fontStyle()     const { return font_style_->currentText(); }
    inline QColor   fontColor()     const { return color_panel_->color(); }

    // cover QWidget::font()
    QFont font() const
    {
        QFont font;
        font.setFamily(fontFamily());
        font.setStyleName(fontStyle());
        font.setPixelSize(fontSize());
        return font;
    }

private:
    QComboBox * font_family_ = nullptr;
    QComboBox * font_style_ = nullptr;
    QComboBox * font_size_ = nullptr;
    ColorPanel* color_panel_ = nullptr;
};

#endif // FONT_MENU_H
