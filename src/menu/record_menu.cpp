#include "record_menu.h"
#include <QGuiApplication>
#include <QScreen>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QTime>

RecordMenu::RecordMenu(bool mm, bool sm, uint8_t buttons, QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);

    window_ = new QWidget(this);
    window_->setObjectName("Window");

    auto *layout_ = new QHBoxLayout();
    layout_->setSpacing(0);
    layout_->setContentsMargins({});

    if (buttons & RecordMenu::MICROPHONE) {
        mic_ = new QCheckBox();
        mic_->setChecked(mm);
        mic_->setObjectName("MicrophoneButton");
        connect(mic_, &QCheckBox::clicked, [this](bool checked) {emit muted(1, checked); });
        layout_->addWidget(mic_);
    }

    if (buttons & RecordMenu::SPEAKER) {
        speaker_ = new QCheckBox();
        speaker_->setChecked(sm);
        speaker_->setObjectName("Speaker");
        connect(speaker_, &QCheckBox::clicked, [this](bool checked) { emit muted(2, checked); });
        layout_->addWidget(speaker_);
    }

    if (buttons & RecordMenu::CAMERA) {
        camera_ = new QCheckBox();
        camera_->setChecked(false);
        camera_->setObjectName("Camera");
        connect(camera_, &QCheckBox::clicked, [this](bool checked) { emit opened(checked); });
        layout_->addWidget(camera_);
    }

    time_label_ = new QLabel("--:--:--");
    time_label_->setFixedSize(85, 40);
    time_label_->setAlignment(Qt::AlignCenter);
    layout_->addWidget(time_label_);
    

    if (buttons & RecordMenu::PAUSE) {
        pause_ = new QCheckBox();
        pause_->setObjectName("PauseButton");
        connect(pause_, &QPushButton::clicked, [this](bool checked) { checked ? emit paused() : emit resumed(); });
        layout_->addWidget(pause_);
    }

    close_btn_ = new QPushButton();
    close_btn_->setObjectName("RecordStopButton");
    connect(close_btn_, &QPushButton::clicked, [this]() { emit stopped(); close(); });
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
    if (pause_) pause_->setChecked(false);

    emit started();

    show();
    move(QGuiApplication::screens().back()->geometry().right() - rect().width() - 5, 100);
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

void RecordMenu::mouseReleaseEvent(QMouseEvent* event)
{
    moving_ = false;
}

void RecordMenu::mute(int type, bool muted)
{
    if (type == 0 && mic_) {
        mic_->setChecked(muted);
    }
    else if (speaker_) {
        speaker_->setChecked(muted);
    }
}

void RecordMenu::camera_checked(bool v)
{
    if (camera_) 
        camera_->setChecked(v);
}

void RecordMenu::disable_cam(bool v) 
{
    if (camera_) {
        camera_->setDisabled(v);
    }
}

void RecordMenu::disable_mic(bool v)
{
    if (mic_) {
        mic_->setDisabled(v);
    }
}