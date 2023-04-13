#include "stylemenu.h"
#include <QFontDatabase>
#include <QLineEdit>
#include <QLayout>
#include "separator.h"
#include "combobox.h"

StyleMenu::StyleMenu(int buttons, QWidget* parent)
    : EditMenu(parent)
{
    auto group = new ButtonGroup(this);

    if (buttons & WIDTH_BTN) {
        width_btn_ = new WidthButton(true, this);
        width_btn_->setObjectName("width-btn");
        width_btn_->setCheckable(true);
        width_btn_->setChecked(true);
        connect(width_btn_, &WidthButton::changed, [this](int w) { width_ = w; fill_ = false; emit changed(); });
        addWidget(width_btn_);
        group->addButton(width_btn_);

        if (buttons & FILL_BTN) {
            fill_btn_ = new QCheckBox(this);
            fill_btn_->setObjectName("fill-btn");
            connect(fill_btn_, &QCheckBox::toggled, [this](bool checked) { fill_ = checked; emit changed(); });

            addWidget(fill_btn_);
            group->addButton(fill_btn_);
        }
    }

    if (buttons & FONT_BTNS) {
        if (buttons & (FILL_BTN | WIDTH_BTN)) {
            addSeparator();
        }

        layout()->setSpacing(3);
        layout()->setContentsMargins({ 5, 0, 0, 0 });

        QFontDatabase font_db;
#if WIN32
        QString family = "微软雅黑";

#else
        QString family = "宋体";
#endif
        // font family
        font_family_ = new ComboBox(this);
        font_family_->add(font_db.families()).setCurrentText(family);
        addWidget(font_family_);

        // font style
        font_style_ = new ComboBox(this);
        font_style_->addItems(font_db.styles(family));
        connect(font_style_, SIGNAL(activated(int)), this, SIGNAL(changed()));
        // set style for family
        connect(font_family_, &QComboBox::currentTextChanged, [=](const QString& family) {
            font_style_->clear();
            font_style_->addItems(font_db.styles(family));
            emit changed();
        });
        addWidget(font_style_);

        // font size
        font_size_ = new ComboBox();
        font_size_->setEditable(true);
        font_size_->lineEdit()->setFocusPolicy(Qt::NoFocus);
        foreach(const int& s, font_db.standardSizes()) {
            font_size_->addItem(QString::number(s));
        }
        font_size_->setCurrentText("16");
        connect(font_size_, SIGNAL(activated(int)), this, SIGNAL(changed()));
        addWidget(font_size_);
    }
    
    if (buttons & COLOR_PENAL) {
        if (buttons & ~COLOR_PENAL) {
            addSeparator();
        }

        color_panel_ = new ColorPanel();
        connect(color_panel_, &ColorPanel::changed, [this](const QColor& c) { color_ = c; emit changed(); });
        addWidget(color_panel_);
    }
}
