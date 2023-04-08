#include "record_menu.h"
#include <QGuiApplication>
#include <QScreen>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QTime>
#include "platform.h"

RecordMenu::RecordMenu(bool mm, bool sm, uint8_t buttons, QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);

    window_ = new QWidget(this);
    window_->setObjectName("menu");

    auto *layout_ = new QHBoxLayout();
    layout_->setSpacing(0);
    layout_->setContentsMargins({});

    if (buttons & RecordMenu::MICROPHONE) {
        mic_btn_ = new QCheckBox();
        mic_btn_->setChecked(mm);
        mic_btn_->setObjectName("mic-btn");
        connect(mic_btn_, &QCheckBox::clicked, [this](bool checked) {emit muted(1, checked); });
        layout_->addWidget(mic_btn_);
    }

    if (buttons & RecordMenu::SPEAKER) {
        speaker_btn_ = new QCheckBox();
        speaker_btn_->setChecked(sm);
        speaker_btn_->setObjectName("speaker-btn");
        connect(speaker_btn_, &QCheckBox::clicked, [this](bool checked) { emit muted(2, checked); });
        layout_->addWidget(speaker_btn_);
    }

    if (buttons & RecordMenu::CAMERA) {
        camera_btn_ = new QCheckBox();
        camera_btn_->setChecked(false);
        camera_btn_->setObjectName("camera-btn");
        connect(camera_btn_, &QCheckBox::clicked, [this](bool checked) { emit opened(checked); });
        layout_->addWidget(camera_btn_);
    }

    time_label_ = new QLabel("--:--:--");
    time_label_->setObjectName("time");
    time_label_->setAlignment(Qt::AlignCenter);
    layout_->addWidget(time_label_);
    

    if (buttons & RecordMenu::PAUSE) {
        pause_btn_ = new QCheckBox();
        pause_btn_->setObjectName("pause-btn");
        connect(pause_btn_, &QCheckBox::clicked, [this](bool checked) { checked ? emit paused() : emit resumed(); });
        layout_->addWidget(pause_btn_);
    }

    close_btn_ = new QCheckBox();
    close_btn_->setObjectName("stop-btn");
    connect(close_btn_, &QCheckBox::clicked, [this]() { emit stopped(); close(); });
    layout_->addWidget(close_btn_);
    
    window_->setLayout(layout_);

    auto *backgroud_layout = new QHBoxLayout();
    backgroud_layout->setSpacing(0);
    backgroud_layout->setContentsMargins({});
    backgroud_layout->addWidget(window_);
    setLayout(backgroud_layout);
}

// in ms
void RecordMenu::time(int64_t time)
{
    time_label_->setText(QTime(0, 0, 0).addMSecs(time).toString("hh:mm:ss"));
}

void RecordMenu::start()
{
    time_label_->setText("00:00:00");
    if (pause_btn_) pause_btn_->setChecked(false);

    emit started();

    show();
    // global position, primary display monitor
    move(platform::display::displays()[0].geometry.right() - width() - 5, 100);
}

void RecordMenu::mousePressEvent(QMouseEvent* event)
{
    begin_pos_ = event->globalPos();
    moving_ = true;
}

void RecordMenu::mouseMoveEvent(QMouseEvent* event)
{
    if (moving_) {
         move(event->globalPos() - begin_pos_ + pos());
        begin_pos_ = event->globalPos();
   }
}

void RecordMenu::mouseReleaseEvent(QMouseEvent*)
{
    moving_ = false;
}

void RecordMenu::mute(int type, bool muted)
{
    if (type == 0 && mic_btn_) {
        mic_btn_->setChecked(muted);
    }
    else if (speaker_btn_) {
        speaker_btn_->setChecked(muted);
    }
}

void RecordMenu::camera_checked(bool v)
{
    if (camera_btn_)
        camera_btn_->setChecked(v);
}

void RecordMenu::disable_cam(bool v) 
{
    if (camera_btn_) {
        camera_btn_->setDisabled(v);
    }
}

void RecordMenu::disable_mic(bool v)
{
    if (mic_btn_) {
        mic_btn_->setDisabled(v);
    }
}

void RecordMenu::disable_speaker(bool v)
{
    if (speaker_btn_) {
        speaker_btn_->setDisabled(v);
    }
}