#include "editing-submenu.h"

#include "separator.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QFontDatabase>
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

        // font
        font_bold_      = new QCheckBox(this);
        font_italic_    = new QCheckBox(this);
        font_underline_ = new QCheckBox(this);
        font_strickout_ = new QCheckBox(this);
        font_family_    = new ComboBox(this);
        font_size_      = new ComboBox(this);

        font_bold_->setObjectName("font-bold");
        font_italic_->setObjectName("font-italic");
        font_underline_->setObjectName("font-underline");
        font_strickout_->setObjectName("font-strickout");

        layout->addWidget(font_bold_);
        layout->addWidget(font_italic_);
        layout->addWidget(font_underline_);
        layout->addWidget(font_strickout_);
        layout->addWidget(font_family_);
        layout->addWidget(font_size_);

        // font family
        font_family_->addItems(QFontDatabase::families());
        connect(font_family_, &QComboBox::currentTextChanged, [this]() { emit fontChanged(font()); });

        // font style
        connect(font_bold_, &QCheckBox::checkStateChanged, [this] { emit fontChanged(font()); });
        connect(font_italic_, &QCheckBox::checkStateChanged, [this] { emit fontChanged(font()); });
        connect(font_underline_, &QCheckBox::checkStateChanged, [this] { emit fontChanged(font()); });
        connect(font_strickout_, &QCheckBox::checkStateChanged, [this] { emit fontChanged(font()); });

        // font size
        font_size_->setEditable(true);
        foreach(const int& s, QFontDatabase::standardSizes()) {
            font_size_->addItem(QString::number(s));
        }
        connect(font_size_, &QComboBox::currentTextChanged, [this] { emit fontChanged(font()); });

        // default font
        setFont(QFontDatabase::font("微软雅黑", "Regular", 16)); // or application's default font
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
    if (font_family_) {
        font_family_->setCurrentText(font.family());
        font_size_->setCurrentText(QString::number(font.pointSizeF(), 'f', 2));
        font_bold_->setChecked(font.bold());
        font_italic_->setChecked(font.italic());
        font_underline_->setChecked(font.underline());
        font_strickout_->setChecked(font.strikeOut());
    }
}

QFont EditingSubmenu::font() const
{
    if (!font_family_ || !font_size_) return QFont{};

    QFont font{};
    font.setFamily(font_family_->currentText());
    font.setPointSizeF(font_size_->currentText().toFloat());
    font.setBold(font_bold_->isChecked());
    font.setItalic(font_italic_->isChecked());
    font.setUnderline(font_underline_->isChecked());
    font.setStrikeOut(font_strickout_->isChecked());
    return font;
}

bool EditingSubmenu::filled() const { return fill_btn_ != nullptr && fill_btn_->isChecked(); }

void EditingSubmenu::fill(bool s)
{
    fill_ = s;

    if (fill_btn_) fill_btn_->setChecked(fill_);
    if (width_btn_) width_btn_->setChecked(!fill_);
}