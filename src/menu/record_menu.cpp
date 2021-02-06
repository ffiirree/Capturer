#include "record_menu.h"
#include <QGuiApplication>
#include <QScreen>
#include <QFont>
#include <QPalette>

RecordMenu::RecordMenu(QWidget* parent)
    : QFrame (parent)
{
    auto last_screen = QGuiApplication::screens().back();
    auto right = last_screen->geometry().right();
    setGeometry(right - 185, 175, 175, MENU_HEIGHT);

    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    layout_ = new QHBoxLayout();
    layout_->setSpacing(0);
    layout_->setMargin(0);
    setLayout(layout_);

    flag_label_ = new QLabel();
    flag_label_->setPixmap(QPixmap::fromImage(QImage(":/icon/res/red_circle.png").scaled(15, 15, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    flag_label_->setAlignment(Qt::AlignCenter);
    flag_label_->setFixedSize(MENU_HEIGHT, MENU_HEIGHT);
    layout_->addWidget(flag_label_);

    time_label_ = new QLabel("00:00:00");
    time_label_->setFixedSize(85, MENU_HEIGHT);
    QFont font;
    font.setPointSize(10);
    font.setFamily("Consolas");
    time_label_->setFont(font);

    QPalette p;
    p.setColor(QPalette::WindowText, Qt::white);
    time_label_->setPalette(p);
    time_label_->setAlignment(Qt::AlignCenter);
    layout_->addWidget(time_label_);

    close_btn_ = new QPushButton();
    close_btn_->setObjectName("RecordStopButton");
    close_btn_->setIcon(QIcon(":/icon/res/stop.png"));
    close_btn_->setIconSize(QSize(24, 24));
    close_btn_->setFixedSize(MENU_HEIGHT, MENU_HEIGHT);
    connect(close_btn_, &QPushButton::clicked, [this](){
        emit STOP();
        close();
    });
    layout_->addWidget(close_btn_);

    // Timer
    timer_ = new QTimer(this);
    timer_->setInterval(1000);

    time_ = new QTime(0, 0, 0);

    connect(timer_, &QTimer::timeout, this, &RecordMenu::update);
}

RecordMenu::~RecordMenu()
{
    delete time_;
}

void RecordMenu::update()
{
    *time_ = time_->addSecs(1);
    time_label_->setText(time_->toString("hh:mm:ss"));
}

void RecordMenu::start()
{
    time_label_->setText("00:00:00");
    *time_ = QTime(0, 0, 0);
    timer_->start();

    this->show();
}

void RecordMenu::stop()
{
    close();

    timer_->stop();
}
