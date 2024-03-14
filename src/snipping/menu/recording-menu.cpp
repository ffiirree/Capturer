#include "recording-menu.h"

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <probe/graphics.h>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QWindow>

#ifdef _WIN32
#include "libcap/win-wgc/win-wgc.h"
#endif

RecordingMenu::RecordingMenu(bool mm, bool sm, uint8_t buttons, QWidget *parent)
    : FramelessWindow(parent, Qt::Tool | Qt::WindowStaysOnTopHint | Qt::WindowTitleHint)
{
    setAttribute(Qt::WA_ShowWithoutActivating);

    const auto layout = new QHBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins({});
    layout->setSizeConstraint(QLayout::SetFixedSize);

    if (buttons & RecordingMenu::AUDIO) {
        // microphone button
        mic_btn_ = new QCheckBox();
        mic_btn_->setChecked(mm);
        mic_btn_->setObjectName("mic-btn");
        connect(mic_btn_, &QCheckBox::clicked, [this](auto checked) { emit muted(1, checked); });
        layout->addWidget(mic_btn_);

        // speaker button
        speaker_btn_ = new QCheckBox();
        speaker_btn_->setChecked(sm);
        speaker_btn_->setObjectName("speaker-btn");
        connect(speaker_btn_, &QCheckBox::clicked, [this](auto checked) { emit muted(2, checked); });
        layout->addWidget(speaker_btn_);
    }

    // time
    time_label_ = new QLabel("--:--:--");
    time_label_->setObjectName("time");
    time_label_->setAlignment(Qt::AlignCenter);
    layout->addWidget(time_label_);

    // pause button
    pause_btn_ = new QCheckBox();
    pause_btn_->setObjectName("pause-btn");
    connect(pause_btn_, &QCheckBox::clicked,
            [this](bool checked) { checked ? emit paused() : emit resumed(); });
    layout->addWidget(pause_btn_);

    // close / stop button
    close_btn_ = new QCheckBox();
    close_btn_->setObjectName("stop-btn");
    connect(close_btn_, &QCheckBox::clicked, [this] {
        emit stopped();
        close();
    });
    layout->addWidget(close_btn_);

#ifdef _WIN32
    // exclude the recording menu
    wgc::ExcludeWindow(reinterpret_cast<HWND>(winId()));
#endif
}

void RecordingMenu::time(const std::chrono::seconds& time)
{
    time_label_->setText(QString::fromStdString(fmt::format("{:%T}", time)));
}

void RecordingMenu::showEvent(QShowEvent *event)
{
    // global position, primary display monitor
    move(probe::graphics::displays()[0].geometry.right() - width() - 12, 100);

    FramelessWindow::showEvent(event);
}

void RecordingMenu::start()
{
    time_label_->setText("00:00:00");
    if (pause_btn_) pause_btn_->setChecked(false);

    emit started();

    show();
}

void RecordingMenu::mute(int type, bool muted)
{
    if (type == 0 && mic_btn_) {
        mic_btn_->setChecked(muted);
    }
    else if (speaker_btn_) {
        speaker_btn_->setChecked(muted);
    }
}

void RecordingMenu::disable_mic(bool v)
{
    if (mic_btn_) {
        mic_btn_->setDisabled(v);
    }
}

void RecordingMenu::disable_speaker(bool v)
{
    if (speaker_btn_) {
        speaker_btn_->setDisabled(v);
    }
}
