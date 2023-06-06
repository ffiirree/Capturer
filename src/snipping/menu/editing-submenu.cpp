#include "editing-submenu.h"

#include "separator.h"

#include <QButtonGroup>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLineEdit>

EditingSubmenu::EditingSubmenu(int buttons, QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::ToolTip)
{
    setCursor(Qt::ArrowCursor);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setVisible(false);

    // frameless: background & border
    auto backgroud_layout = new QHBoxLayout(this);
    backgroud_layout->setSpacing(0);
    backgroud_layout->setContentsMargins({});

    auto background = new QWidget(this);
    background->setObjectName("editing-submenu");
    backgroud_layout->addWidget(background);

    auto layout = new QHBoxLayout(background);
    layout->setSpacing(0);
    layout->setContentsMargins({});

    //
    auto group = new QButtonGroup(this);

    if (buttons & WIDTH_BTN) {
        // pen: width
        width_btn_ = new WidthButton(true, this);
        width_btn_->setObjectName("width-btn");
        width_btn_->setChecked(true);
        connect(width_btn_, &WidthButton::changed, [this](int w) {
            pen_.setWidth(w);
            emit penChanged(pen_);
        });
        layout->addWidget(width_btn_);
        group->addButton(width_btn_);

        // brush
        if (buttons & FILL_BTN) {
            fill_btn_ = new QCheckBox(this);
            fill_btn_->setObjectName("fill-btn");
            connect(fill_btn_, &QCheckBox::toggled, [this](bool checked) { emit fillChanged(checked); });

            layout->addWidget(fill_btn_);
            group->addButton(fill_btn_);
        }
    }

    // font
    if (buttons & FONT_BTNS) {
        if (buttons & (FILL_BTN | WIDTH_BTN)) {
            layout->addWidget(new Separator());
        }

        layout->setSpacing(3);
        layout->setContentsMargins({ 5, 0, 0, 0 });

        // foont
        font_family_ = new ComboBox(this);
        font_style_  = new ComboBox(this);
        font_size_   = new ComboBox(this);
        layout->addWidget(font_family_);
        layout->addWidget(font_style_);
        layout->addWidget(font_size_);

        QFontDatabase font_db;
#if WIN32
        font_.setFamily("微软雅黑");
#else
        font_.setFamily("宋体");
#endif
        font_.setPointSizeF(16);

        // font family
        font_family_->addItems(font_db.families());
        font_family_->setCurrentText(font_.family());
        connect(font_family_, &QComboBox::currentTextChanged, [=, this](const QString& family) {
            font_.setFamily(family);
            font_style_->clear();
            font_style_->addItems(font_db.styles(family));
            emit fontChanged(font_);
        });

        // font style
        font_style_->addItems(font_db.styles(font_.family()));
        font_style_->setCurrentText(font_.styleName());
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
        connect(font_size_, QOverload<int>::of(&QComboBox::currentIndexChanged), [=, this](auto) {
            font_.setPointSizeF(font_size_->currentText().toFloat());
            emit fontChanged(font_);
        });
    }

    // pen: color
    if (buttons & COLOR_PENAL) {
        if (buttons & ~COLOR_PENAL) {
            layout->addWidget(new Separator());
        }

        color_panel_ = new ColorPanel();
        connect(color_panel_, &ColorPanel::changed, [this](const QColor& c) {
            if (filled()) {
                brush_ = c;
                emit brushChanged(brush_);
            }
            else {
                pen_.setColor(c);
                emit penChanged(pen_);
            }
        });
        layout->addWidget(color_panel_);
    }
}

void EditingSubmenu::setPen(const QPen& pen)
{
    pen_ = pen;
    if ((!filled() || (filled() && brush() == Qt::NoBrush)) && color_panel_)
        color_panel_->setColor(pen_.color());
    if (width_btn_) width_btn_->setValue(pen_.width());
}

void EditingSubmenu::setBrush(const QBrush& brush)
{
    brush_ = brush;
    if (filled() && brush != Qt::NoBrush && color_panel_) color_panel_->setColor(brush.color());
}

void EditingSubmenu::setFont(const QFont& font)
{
    font_ = font;
    if (font_family_) font_family_->setCurrentText(font.family());
    if (font_size_) font_size_->setCurrentText(QString::number(font.pointSizeF(), 'f', 2));
    if (font_style_) font_style_->setCurrentText(font.styleName());
}

bool EditingSubmenu::filled() const { return fill_btn_ ? fill_btn_->isChecked() : false; }

void EditingSubmenu::fill(bool s)
{
    fill_ = s;

    if (fill_btn_) fill_btn_->setChecked(fill_);
    if (width_btn_) width_btn_->setChecked(!fill_);
}