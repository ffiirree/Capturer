#include "control-widget.h"

#include "combobox.h"

#include <chrono>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>

ControlWidget::ControlWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);

    // controller
    auto layout = new QVBoxLayout();
    layout->setSpacing(0);
    layout->setContentsMargins({});
    setLayout(layout);

    // title bar
    {
        auto title_bar = new QWidget();
        title_bar->setObjectName("title-bar");
        layout->addWidget(title_bar);

        auto hl = new QHBoxLayout();
        hl->setSpacing(0);
        hl->setContentsMargins({});
        title_bar->setLayout(hl);

        hl->addSpacerItem(new QSpacerItem(16, 10, QSizePolicy::Minimum, QSizePolicy::Minimum));

        auto title = new QLabel("Capturer Player");
        title->setObjectName("title-label");
        hl->addWidget(title);
        connect(parent, &QWidget::windowTitleChanged, [=](const auto& t) { title->setText(t); });
        hl->addSpacerItem(new QSpacerItem(10, 10, QSizePolicy::Expanding, QSizePolicy::Minimum));

        auto close_btn = new QCheckBox();
        close_btn->setObjectName("close-btn");
        connect(close_btn, &QCheckBox::clicked, parent, &QWidget::close);
        hl->addWidget(close_btn);
    }

    layout->addSpacerItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding));

    // control
    {
        auto control = new QWidget();
        control->setMouseTracking(true);
        control->setObjectName("control-bar");
        layout->addWidget(control);

        auto vl = new QVBoxLayout();
        vl->setSpacing(0);
        vl->setContentsMargins({ 10, 0, 10, 10 });
        control->setLayout(vl);

        time_slider_ = new Slider(Qt::Horizontal);
        time_slider_->setObjectName("time-slider");
        time_slider_->setMouseTracking(true);
        time_slider_->setRange(1, 100);
        connect(time_slider_, &Slider::seek, this, &ControlWidget::seek);
        vl->addWidget(time_slider_);

        auto hl = new QHBoxLayout();
        hl->setSpacing(10);
        hl->setContentsMargins({});
        vl->addLayout(hl);

        pause_btn_ = new QCheckBox();
        pause_btn_->setObjectName("pause-btn");
        connect(pause_btn_, &QCheckBox::stateChanged, [this](int state) { state ? pause() : resume(); });
        connect(this, &ControlWidget::pause, [this]() {
            if (!pause_btn_->isChecked()) pause_btn_->setChecked(true);
        });
        connect(this, &ControlWidget::resume, [this]() {
            if (pause_btn_->isChecked()) pause_btn_->setChecked(false);
        });
        hl->addWidget(pause_btn_);

        // time
        time_label_ = new QLabel("--:--:--");
        time_label_->setObjectName("time-label");
        time_label_->setAlignment(Qt::AlignCenter);
        hl->addWidget(time_label_);

        hl->addWidget(new QLabel("/"));

        duration_label_ = new QLabel("--:--:--");
        duration_label_->setObjectName("duration-label");
        duration_label_->setAlignment(Qt::AlignCenter);
        hl->addWidget(duration_label_);

        hl->addSpacerItem(new QSpacerItem(10, 10, QSizePolicy::Expanding, QSizePolicy::Maximum));

        // speed
        auto speed_box = new ComboBox();
        speed_box->setObjectName("speed-box");
        speed_box
            ->add({
                { 0.5, "0.5x" },
                { 0.75, "0.75x" },
                { 1.0, "1.0x" },
                { 1.25, "1.25x" },
                { 1.5, "1.5x" },
                { 2.0, "2.0x" },
                { 3.0, "3.0x" },
            })
            .onselected([this](const QVariant& v) { emit speed(v.toFloat()); })
            .select(1.0);
        hl->addWidget(speed_box);

        // volume
        volume_btn_ = new QCheckBox();
        volume_btn_->setObjectName("volume-btn");
        hl->addWidget(volume_btn_);

        volume_slider_ = new Slider(Qt::Horizontal);
        volume_slider_->setObjectName("volume-bar");
        volume_slider_->setFixedWidth(125);
        volume_slider_->setRange(0, 100);
        connect(volume_slider_, &Slider::seek, [this](int v) {
            volume_btn_->setChecked(v == 0);
            emit volume(v);
        });
        hl->addWidget(volume_slider_);

        // settings
        auto setting_btn = new QCheckBox();
        setting_btn->setObjectName("setting-btn");
        setting_btn->setCheckable(false);
        hl->addWidget(setting_btn);
    }

    setVolume(0);
}

void ControlWidget::setDuration(int64_t time)
{
    time_slider_->setMaximum(static_cast<int>(time / 1'000));
    duration_label_->setText(fmt::format("{:%H:%M:%S}", std::chrono::milliseconds{ time / 1'000 }).c_str());
}

void ControlWidget::setTime(int64_t time)
{
    if (!time_slider_->isSliderDown()) {
        time_slider_->setValue(static_cast<int>(time / 1'000));
    }
    time_label_->setText(fmt::format("{:%H:%M:%S}", std::chrono::milliseconds{ time / 1'000 }).c_str());
}

void ControlWidget::setVolume(int v)
{
    v = std::clamp<int>(v, 0, volume_slider_->maximum());

    if (!volume_slider_->isSliderDown()) {
        volume_slider_->setValue(v);
    }
    volume_btn_->setChecked(v == 0);
}

bool ControlWidget::paused() const { return pause_btn_->isChecked(); }