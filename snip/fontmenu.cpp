#include "fontmenu.h"
#include <QComboBox>
#include <QFontDatabase>
#include "colorbutton.h"

FontMenu::FontMenu(QWidget *parent)
    : QFrame(parent)
{
    setCursor(Qt::ArrowCursor);

    const int HEIGHT = 30;
    setFixedHeight(HEIGHT);

    layout_ = new QHBoxLayout();
    layout_->setSpacing(3);
    layout_->setMargin(0);
    this->setLayout(layout_);

    auto font_family = new QComboBox();
    font_family->setFixedSize(135, HEIGHT - 6);
    QFontDatabase font_db;
    foreach (const QString &family, font_db.families()) {
        font_family->addItem(family);
    }
    layout()->addWidget(font_family);


    auto font_style = new QComboBox();
    font_style->setFixedSize(90, HEIGHT - 6);
    foreach (const QString &style, font_db.styles(font_db.families()[0])) {
        font_style->addItem(style);
    }
    layout()->addWidget(font_style);

    connect(font_style, &QComboBox::currentTextChanged, this, &FontMenu::styleChanged);

    connect(font_family, &QComboBox::currentTextChanged, [=](const QString &family){
        font_style->clear();
        foreach (const QString &style, font_db.styles(family)) {
            font_style->addItem(style);
        }
        emit familyChanged(family);
    });

    auto font_size = new QComboBox();
    font_size->setFixedSize(45, HEIGHT - 6);
    foreach (const int &s, font_db.standardSizes()) {
        font_size->addItem(QString::number(s));
    }
    layout()->addWidget(font_size);
    connect(font_size, &QComboBox::currentTextChanged, [=](const QString& s){ emit sizeChanged(s.toInt()); });

    auto color_btn = new ColorButton();
    connect(color_btn, &ColorButton::changed, this, &FontMenu::colorChanged);
    color_btn->setFixedSize(HEIGHT - 6, HEIGHT - 6);
    layout()->addWidget(color_btn);

    hide();
}
