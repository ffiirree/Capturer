#include "editing-submenu.h"

#include "separator.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLineEdit>

#ifdef Q_OS_WIN
#include "platforms/window-effect.h"
#endif

EditingSubmenu::EditingSubmenu(int buttons, QWidget *parent)
    : FramelessWindow(parent, Qt::ToolTip | Qt::WindowStaysOnTopHint)
{
    setCursor(Qt::ArrowCursor);
    setAttribute(Qt::WA_ShowWithoutActivating);
    QWidget::setVisible(false);

#ifdef Q_OS_WIN
    windows::dwm::set_window_corner(reinterpret_cast<HWND>(winId()), DWMWCP_DONOTROUND);
#endif

    const auto layout = new QHBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins({});
    layout->setSizeConstraint(QLayout::SetFixedSize);

    //
    const auto group = new QButtonGroup(this);

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

        layout->setSpacing(0);
        layout->setContentsMargins({});

        // foont
        font_family_ = new ComboBox(this);
        font_style_  = new ComboBox(this);
        font_size_   = new ComboBox(this);
        layout->addWidget(font_family_);
        layout->addWidget(font_style_);
        layout->addWidget(font_size_);

        // font family
        font_family_->addItems(fonts_.families());
        connect(font_family_, &QComboBox::currentTextChanged, [this](const QString& family) {
            font_style_->clear();
            font_style_->addItems(fonts_.styles(family));
            emit fontChanged(font());
        });

        // font style
        connect(font_style_, &QComboBox::currentTextChanged, [this] { emit fontChanged(font()); });

        // font size
        font_size_->setEditable(true);
        foreach(const int& s, fonts_.standardSizes()) {
            font_size_->addItem(QString::number(s));
        }
        connect(font_size_, &QComboBox::currentTextChanged, [this] { emit fontChanged(font()); });

        // default font
        setFont(fonts_.font("微软雅黑", "Regular", 16)); // or application's default font
    }

    // pen: color
    if (buttons & COLOR_PENAL) {
        if (buttons & ~COLOR_PENAL) {
            layout->addWidget(new Separator());
        }

        color_panel_ = new ColorPanel();
        connect(color_panel_, &ColorPanel::changed, [this](const QColor& c) {
            pen_.setColor(c);
            emit penChanged(pen_);
        });
        layout->addWidget(color_panel_);
    }
}

void EditingSubmenu::setPen(const QPen& pen, bool silence)
{
    if (color_panel_) {
        color_panel_->setColor(pen.color(), silence);
        pen_.setColor(color_panel_->color());
    }

    if (width_btn_) {
        width_btn_->setValue(pen.width(), silence);
        pen_.setWidth(width_btn_->value());
    }
}

void EditingSubmenu::setFont(const QFont& font)
{
    if (font_family_) font_family_->setCurrentText(font.family());
    if (font_size_) font_size_->setCurrentText(QString::number(font.pointSizeF(), 'f', 2));
    if (font_style_) font_style_->setCurrentText(font.styleName());
}

QFont EditingSubmenu::font() const
{
    if (!font_family_ || !font_style_ || !font_size_) return QFont{};

    auto font = fonts_.font(font_family_->currentText(), font_style_->currentText(), 16);
    font.setPointSizeF(font_size_->currentText().toFloat());
    return font;
}

bool EditingSubmenu::filled() const { return fill_btn_ ? fill_btn_->isChecked() : false; }

void EditingSubmenu::fill(bool s)
{
    fill_ = s;

    if (fill_btn_) fill_btn_->setChecked(fill_);
    if (width_btn_) width_btn_->setChecked(!fill_);
}