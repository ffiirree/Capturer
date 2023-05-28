#include "stylemenu.h"

#include "colorpanel.h"
#include "combobox.h"
#include "separator.h"
#include "widthbutton.h"

#include <QButtonGroup>
#include <QFontDatabase>
#include <QLayout>
#include <QLineEdit>

StyleMenu::StyleMenu(int buttons, QWidget *parent)
    : EditMenu(parent)
{
    auto group = new QButtonGroup(this);

    if (buttons & WIDTH_BTN) {
        // pen: width
        width_btn_ = new WidthButton(true, this);
        width_btn_->setObjectName("width-btn");
        width_btn_->setChecked(true);
        connect(width_btn_, &WidthButton::changed, [this](int w) { pen_.setWidth(w); emit penChanged(pen_); });
        addWidget(width_btn_);
        group->addButton(width_btn_);

        // brush
        if (buttons & FILL_BTN) {
            fill_btn_ = new QCheckBox(this);
            fill_btn_->setObjectName("fill-btn");
            connect(fill_btn_, &QCheckBox::toggled, [this](bool checked) { emit fillChanged(checked); });

            addWidget(fill_btn_);
            group->addButton(fill_btn_);
        }
    }

    // font
    if (buttons & FONT_BTNS) {
        if (buttons & (FILL_BTN | WIDTH_BTN)) {
            addSeparator();
        }

        layout()->setSpacing(3);
        layout()->setContentsMargins({ 5, 0, 0, 0 });

        // foont
        font_family_ = new ComboBox(this);
        font_style_  = new ComboBox(this);
        font_size_   = new ComboBox(this);
        addWidget(font_family_);
        addWidget(font_style_);
        addWidget(font_size_);

        QFontDatabase font_db;
#if WIN32
        QString family = "微软雅黑";

#else
        QString family = "宋体";
#endif
        // font family
        font_family_->addItems(font_db.families());
        font_family_->setCurrentText(family);
        connect(font_family_, &QComboBox::currentTextChanged, [=, this](const QString& family) {
            font_.setFamily(family);
            font_style_->clear();
            font_style_->addItems(font_db.styles(family));
            emit fontChanged(font_);
        });

        // font style
        font_style_->addItems(font_db.styles(family));
        connect(font_style_, &QComboBox::currentTextChanged, [=, this](const QString& style) {
            font_.setStyleName(style);
            emit fontChanged(font_);
        });

        // font size
        font_size_->setEditable(true);
        font_size_->lineEdit()->setFocusPolicy(Qt::NoFocus);
        foreach(const int& s, font_db.standardSizes()) {
            font_size_->addItem(QString::number(s));
        }
        font_size_->setCurrentText("16");
        connect(font_size_, &QComboBox::currentTextChanged, [=, this](const QString& size) {
            font_.setPointSizeF(size.toFloat());
            emit fontChanged(font_);
        });
    }

    // pen: color
    if (buttons & COLOR_PENAL) {
        if (buttons & ~COLOR_PENAL) {
            addSeparator();
        }

        color_panel_ = new ColorPanel();
        connect(color_panel_, &ColorPanel::changed, [this](const QColor& c) {
            if (fill()) {
                brush_ = c;
                emit brushChanged(brush_);
            }
            else {
                pen_.setColor(c);
                emit penChanged(pen_);
            }
        });
        addWidget(color_panel_);
    }
}

void StyleMenu::pen(const QPen& pen)
{
    pen_ = pen;
    if (color_panel_) color_panel_->setColor(pen_.color());
    if (width_btn_) width_btn_->setValue(pen_.width());
}

void StyleMenu::brush(const QBrush& brush)
{
    brush_ = brush;

    if (fill_btn_) fill_btn_->setChecked(brush != QBrush{});
    if (width_btn_) width_btn_->setChecked(brush == QBrush{});
}

void StyleMenu::font(const QFont& font)
{
    font_ = font;
    if (font_family_) font_family_->setCurrentText(font.family());
    if (font_size_) font_size_->setCurrentText(QString::number(font.pointSizeF(), 'f', 2));
    if (font_style_) font_style_->setCurrentText(font.styleName());
}

bool StyleMenu::fill() const { return fill_btn_ ? fill_btn_->isChecked() : false; }

void StyleMenu::fill(bool s)
{
    if (fill_btn_) fill_btn_->setCheckState(s ? Qt::Checked : Qt::Unchecked);
}