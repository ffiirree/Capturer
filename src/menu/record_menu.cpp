#include "record_menu.h"
#include <QGuiApplication>
#include <QScreen>
#include <QFont>
#include <QPalette>
#include <QMouseEvent>
#include <QHBoxLayout>

RecordMenu::RecordMenu(bool mm, bool sm, uint8_t buttons, QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);

    window_ = new QWidget(this);
    window_->setObjectName("Window");

    auto *layout_ = new QHBoxLayout();
    layout_->setSpacing(0);
    layout_->setMargin(0);

    if (buttons & RECORD_MENU_M_MUTE) {
        mic_ = new QCheckBox();
        mic_->setChecked(mm);
        mic_->setObjectName("MicrophoneButton");
        connect(mic_, &QPushButton::clicked, [this](bool checked) {emit muted(1, checked); });
        layout_->addWidget(mic_);
    }

    if (buttons & RECORD_MENU_S_MUTE) {
        speaker_ = new QCheckBox();
        speaker_->setChecked(sm);
        speaker_->setObjectName("Speaker");
        connect(speaker_, &QPushButton::clicked, [this](bool checked) { emit muted(2, checked); });
        layout_->addWidget(speaker_);
    }

    time_label_ = new QLabel("--:--:--");
    time_label_->setFixedSize(85, 40);
    time_label_->setAlignment(Qt::AlignCenter);
    layout_->addWidget(time_label_);
    

    if (buttons & RECORD_MENU_PAUSE) {
        pause_ = new QCheckBox();
        pause_->setObjectName("PauseButton");
        connect(pause_, &QPushButton::clicked, [this](bool checked) { checked ? pause() : resume(); });
        layout_->addWidget(pause_);
    }

    close_btn_ = new QPushButton();
    close_btn_->setObjectName("RecordStopButton");
    connect(close_btn_, &QPushButton::clicked, [this]() { emit stopped(); close(); });
    layout_->addWidget(close_btn_);
    
    window_->setLayout(layout_);

    auto *backgroud_layout = new QHBoxLayout();
    backgroud_layout->setSpacing(0);
    backgroud_layout->setMargin(0);
    backgroud_layout->addWidget(window_);
    setLayout(backgroud_layout);

    // Timer
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &RecordMenu::update);
}

void RecordMenu::update()
{
    time_label_->setText(QTime(0, 0, 0).addMSecs(counter_ + time_.elapsed()).toString("hh:mm:ss"));
}

void RecordMenu::start()
{
    time_label_->setText("00:00:00");
    time_.restart();
    timer_->start(100);
    counter_ = 0;

    emit started();

    show();
    move(QGuiApplication::screens().back()->geometry().right() - rect().width() - 5, 100);
}

void RecordMenu::pause() 
{
    timer_->stop();
    counter_ += time_.elapsed();

    emit paused();
}

void RecordMenu::resume()
{
    time_.restart();
    timer_->start();
    
    emit resumed();
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