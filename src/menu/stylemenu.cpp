#include "stylemenu.h"
#include <QComboBox>
#include <QFontDatabase>
#include <QListView>
#include <QLineEdit>
#include <QLayout>
#include "separator.h"

StyleMenu::StyleMenu(int buttons, QWidget* parent)
    : EditMenu(parent)
{
    ButtonGroup* group = new ButtonGroup(this);

    if (buttons & WIDTH_BTN) {
        width_btn_ = new WidthButton({ HEIGHT, HEIGHT }, 3, true);
        connect(width_btn_, &WidthButton::changed, [this](int w) { width_ = w; fill_ = false; emit changed(); });
        addButton(width_btn_);
        width_btn_->setChecked(true);
        group->addButton(width_btn_);

        if (buttons & FILL_BTN) {
            fill_btn_ = new IconButton(QPixmap(":/icon/res/fill"), { HEIGHT, HEIGHT }, { ICON_W, ICON_W }, true);
            connect(fill_btn_, &IconButton::toggled, [this](bool checked) { fill_ = checked; emit changed(); });

            addButton(fill_btn_);
            group->addButton(fill_btn_);
        }
    }

    if (buttons & FONT_BTNS) {
        if (buttons & (FILL_BTN | WIDTH_BTN)) {
            addSeparator();
        }

        layout()->setSpacing(3);
        layout()->setContentsMargins({ 5, 0, 0, 0 });

        // font family
        font_family_ = new QComboBox(this);
        font_family_->setView(new QListView());
        font_family_->view()->window()->setWindowFlag(Qt::FramelessWindowHint);
        font_family_->view()->window()->setWindowFlag(Qt::NoDropShadowWindowHint);
        font_family_->view()->window()->setAttribute(Qt::WA_TranslucentBackground);
        font_family_->setFixedSize(125, HEIGHT - 6);
        QFontDatabase font_db;
        foreach(const QString & family, font_db.families()) {
            font_family_->addItem(family);

#if WIN32
            if (family == "微软雅黑") {
                font_family_->setCurrentText(family);
            }
#else
            if (family == "宋体") {
                font_family_->setCurrentText(family);
            }
#endif
        }
        addWidget(font_family_);

        // font style
        font_style_ = new QComboBox(this);
        font_style_->setView(new QListView());
        font_style_->view()->window()->setWindowFlag(Qt::FramelessWindowHint);
        font_style_->view()->window()->setWindowFlag(Qt::NoDropShadowWindowHint);
        font_style_->view()->window()->setAttribute(Qt::WA_TranslucentBackground);
        font_style_->setFixedSize(100, HEIGHT - 6);
        foreach(const QString & style, font_db.styles(font_db.families()[0])) {
            font_style_->addItem(style);
        }
        connect(font_style_, SIGNAL(activated(int)), this, SIGNAL(changed()));
        // set style for family
        connect(font_style_, &QComboBox::currentTextChanged, [=](const QString& family) {
            font_style_->clear();
            foreach(const QString & style, font_db.styles(family)) {
                font_style_->addItem(style);
            }
            emit changed();
        });
        addWidget(font_style_);

        // font size
        font_size_ = new QComboBox();
        font_size_->setView(new QListView());
        font_size_->setEditable(true);
        font_size_->lineEdit()->setFocusPolicy(Qt::NoFocus);
        font_size_->view()->window()->setWindowFlag(Qt::FramelessWindowHint);
        font_size_->view()->window()->setWindowFlag(Qt::NoDropShadowWindowHint);
        font_size_->view()->window()->setAttribute(Qt::WA_TranslucentBackground);
        font_size_->setFixedSize(85, HEIGHT - 6);
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
