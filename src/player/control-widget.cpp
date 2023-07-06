#include "control-widget.h"

#include <chrono>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <QHBoxLayout>
#include <QVBoxLayout>

ControlWidget::ControlWidget(QWidget *parent)
    : QWidget(parent)
{
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
        auto vl      = new QVBoxLayout();
        vl->setSpacing(0);
        vl->setContentsMargins({ 10, 0, 10, 0 });
        control->setObjectName("control-bar");
        control->setLayout(vl);

        time_slider_ = new QSlider(Qt::Horizontal);
        time_slider_->setRange(1, 100);
        time_slider_->setValue(10);
        vl->addWidget(time_slider_);

        auto hl = new QHBoxLayout();
        hl->setSpacing(0);
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

        // volume
        //auto volume_btn = new QCheckBox();
        //volume_btn->setObjectName("volume-btn");
        //hl->addWidget(volume_btn);

        //volume_slider_ = new QSlider(Qt::Horizontal);
        //volume_slider_->setObjectName("volume-bar");
        //volume_slider_->setFixedWidth(125);
        //volume_slider_->setRange(1, 100);
        //volume_slider_->setValue(25);
        //hl->addWidget(volume_slider_);

        layout->addWidget(control);
    }
}

void ControlWidget::setDuration(int64_t time)
{
    time_slider_->setMaximum(time);
    duration_label_->setText(fmt::format("{:%H:%M:%S}", std::chrono::seconds{ time }).c_str());
}

void ControlWidget::setTime(int64_t time)
{
    time_slider_->setValue(time);
    time_label_->setText(fmt::format("{:%H:%M:%S}", std::chrono::seconds{ time }).c_str());
}

bool ControlWidget::paused() const { return pause_btn_->isChecked(); }