#include "texteditmenu.h"
#include <QFontDatabase>

FontMenu::FontMenu(QWidget *parent)
    : EditMenu(parent)
{
    layout()->setSpacing(3);

    // font family
    font_family_ = new QComboBox();
    font_family_->setFixedSize(120, HEIGHT - 6);
    QFontDatabase font_db;
    foreach (const QString &family, font_db.families()) {
        font_family_->addItem(family);
        if(family == "宋体"){
            font_family_->setCurrentText(family);
        }
    }
    addWidget(font_family_);

    // font style
    font_style_ = new QComboBox();
    font_style_->setFixedSize(90, HEIGHT - 6);
    foreach (const QString &style, font_db.styles(font_db.families()[0])) {
        font_style_->addItem(style);
    }
    connect(font_style_, &QComboBox::currentTextChanged, this, &FontMenu::changed);
    // set style for family
    connect(font_family_, &QComboBox::currentTextChanged, [=](const QString &family){
        font_style_->clear();
        foreach (const QString &style, font_db.styles(family)) {
            font_style_->addItem(style);
        }
        emit changed();
    });
    addWidget(font_style_);

    // font size
    font_size_ = new QComboBox();
    font_size_->setFixedSize(45, HEIGHT - 6);
    foreach (const int &s, font_db.standardSizes()) {
        font_size_->addItem(QString::number(s));
        if(s == 16) {
            font_size_->setCurrentText(QString::number(s));
        }
    }
    connect(font_size_, &QComboBox::currentTextChanged, this, &FontMenu::changed);
    addWidget(font_size_);

    // font color
    color_panel_ = new ColorPanel();
    color_panel_->setFixedHeight(HEIGHT - 6);
    connect(color_panel_, &ColorPanel::changed, [this](const QColor& color) {
        pen_.setColor(color);
        emit changed();
    });
    addWidget(color_panel_);

    pen_ = QPen(color_panel_->color());
}
