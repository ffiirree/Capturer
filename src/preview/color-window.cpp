#include "color-window.h"

#include "clipboard.h"
#include "navigation-bar.h"

#include <fmt/format.h>
#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QShortcut>
#include <QVBoxLayout>
#include <string>

// clang-format off
static std::string to_hex_string(const QColor& color)
{
    return fmt::format("#{:02X}{:02X}{:02X}", color.red(), color.green(), color.blue());
}

static std::string to_rgb_string(const QColor& color, number_t format = number_t::integral)
{
    switch (format) {
    case number_t::floating: return fmt::format("{:4.2f}, {:4.2f}, {:4.2f}", color.redF(), color.greenF(), color.blueF());
    default:                 return fmt::format("{:3d}, {:3d}, {:3d}", color.red(), color.green(), color.blue());
    }
}

static std::string to_bgr_string(const QColor& color, number_t format = number_t::integral)
{
    switch (format) {
    case number_t::floating: return fmt::format("{:4.2f}, {:4.2f}, {:4.2f}", color.blueF(), color.greenF(), color.redF());
    default:                 return fmt::format("{:3d}, {:3d}, {:3d}", color.blue(), color.green(), color.red());
    }
}

static std::string to_hsv_string(const QColor& color, number_t format = number_t::integral)
{
    switch (format) {
    case number_t::integral: return fmt::format("{:3d}, {:3d}, {:3d}", color.hsvHue(), color.hsvSaturation(), color.value());
    case number_t::floating: return fmt::format("{:4.2f}, {:4.2f}, {:4.2f}", color.hsvHueF(), color.hsvSaturationF(), color.valueF());
    default:                 return fmt::format("{:3d}, {:4.1f}, {:4.1f}", color.hsvHue(), color.hsvSaturationF() * 100, color.valueF() * 100);
    }
}

static std::string to_hsl_string(const QColor& color, number_t format = number_t::integral)
{
    switch (format) {
    case number_t::integral: return fmt::format("{:3d}, {:3d}, {:3d}", color.hslHue(), color.hslSaturation(), color.value());
    case number_t::floating: return fmt::format("{:4.2f}, {:4.2f}, {:4.2f}", color.hslHueF(), color.hslSaturationF(), color.valueF());
    default:                 return fmt::format("{:3d}, {:4.1f}, {:4.1f}", color.hslHue(), color.hslSaturationF() * 100, color.valueF() * 100);
    }
}
// clang-format on

ColorWindow::ColorWindow(QWidget *parent)
    : ColorWindow({}, parent)
{}

ColorWindow::ColorWindow(const std::shared_ptr<QMimeData>& data, QWidget *parent)
    : FramelessWindow(parent)
{
    setWindowTitle("Color Window");

    if (data && data->hasColor()) {
        data_ = data;
        data_->setData(clipboard::MIME_TYPE_STATUS, "P");
        color_ = data_->colorData().value<QColor>();
    }

    setWindowFlags((windowFlags() & ~Qt::Window) | Qt::Tool | Qt::WindowStaysOnTopHint);
    connect(new QShortcut(Qt::Key_Escape, this), &QShortcut::activated, [this]() { close(); });

    auto layout = new QVBoxLayout();
    setLayout(layout);

    auto hl = new QHBoxLayout();
    layout->addLayout(hl);

    auto nbar = new NavigationBar(this);
    nbar->add(new QCheckBox("Integral"), static_cast<int>(number_t::integral));
    nbar->add(new QCheckBox("Hybrid"), static_cast<int>(number_t::hybrid));
    nbar->add(new QCheckBox("Floating"), static_cast<int>(number_t::floating));
    hl->addStretch();
    hl->addWidget(nbar);
    hl->addStretch();

    connect(nbar, &NavigationBar::toggled, [this](int id) { refresh(static_cast<number_t>(id)); });

    //
    layout->addStretch();

    auto gl = new QGridLayout();
    layout->addLayout(gl);
    // hex
    gl->addWidget(new QLabel("HEX"), 0, 0);

    hex_ = new QLineEdit();
    hex_->setAlignment(Qt::AlignCenter);
    gl->addWidget(hex_, 0, 1);

    // rgb
    gl->addWidget(new QLabel("RGB"), 1, 0);
    rgb_ = new QLineEdit();
    rgb_->setAlignment(Qt::AlignCenter);
    gl->addWidget(rgb_, 1, 1);

    gl->addWidget(new QLabel("BGR"), 2, 0);
    bgr_ = new QLineEdit();
    bgr_->setAlignment(Qt::AlignCenter);
    gl->addWidget(bgr_, 2, 1);

    // hsv
    gl->addWidget(new QLabel("HSV"), 3, 0);
    hsv_ = new QLineEdit();
    hsv_->setAlignment(Qt::AlignCenter);
    gl->addWidget(hsv_, 3, 1);

    // hsl
    gl->addWidget(new QLabel("HSL"), 4, 0);
    hsl_ = new QLineEdit();
    hsl_->setAlignment(Qt::AlignCenter);
    gl->addWidget(hsl_, 4, 1);

    nbar->setId(static_cast<int>(number_t::hybrid));
}

void ColorWindow::refresh(number_t format)
{
    format_ = format;

    hex_->setText(QString::fromStdString(to_hex_string(color_)));
    rgb_->setText(QString::fromStdString(to_rgb_string(color_, format_)));
    bgr_->setText(QString::fromStdString(to_bgr_string(color_, format_)));
    hsv_->setText(QString::fromStdString(to_hsv_string(color_, format_)));
    hsl_->setText(QString::fromStdString(to_hsl_string(color_, format_)));
}

void ColorWindow::closeEvent(QCloseEvent *event)
{
    if (data_) data_->setData(clipboard::MIME_TYPE_STATUS, "N");
    return FramelessWindow::closeEvent(event);
}