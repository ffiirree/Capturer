#include "settingdialog.h"

SettingDialog::SettingDialog(QWidget * parent)
    : QDialog(parent)
{
    setFixedSize(800, 600);

    btn_group = new QButtonGroup(this);

    menu_ = new QWidget(this);
    menu_->setGeometry(0, 0, 250, 600);
    menu_layout_ = new QVBoxLayout();
    menu_->setLayout(menu_layout_);


    auto shortcut_btn = new QRadioButton("Shortcut", this);
    btn_group->addButton(shortcut_btn);

    auto about_action = new QRadioButton("About Capturer", this);
    btn_group->addButton(about_action);

    menu_layout_->addWidget(shortcut_btn);
    menu_layout_->addWidget(about_action);
    menu_layout_->addStretch();

    scroll_area_ = new QScrollArea(this);
    scroll_area_->setGeometry(250, 0, 550, 600);
    scroll_area_->setBackgroundRole(QPalette::Dark);
    scroll_layout_ = new QVBoxLayout();

    scroll_widget_ = new QWidget(this);

    shortcut_widget_ = new QWidget(this);
    shortcut_widget_->setFixedSize(500, 500);
    shortcut_widget_->setStyleSheet("background-color:red;");
    shortcut_widget_->show();;

    about_widget_ = new QWidget(this);
    about_widget_->setFixedSize(500, 500);
    about_widget_->setStyleSheet("background-color:blue;");
    about_widget_->show();

    scroll_layout_->addWidget(shortcut_widget_);
    scroll_layout_->addWidget(about_widget_);

    scroll_layout_->setMargin(0);
    scroll_widget_->setLayout(scroll_layout_);
    scroll_area_->setWidget(scroll_widget_);

    //
    connect(shortcut_btn, &QRadioButton::clicked, [&]() {
        scroll_area_->verticalScrollBar()->setValue(0);
    });
    connect(about_action, &QRadioButton::clicked, [&]() {
        scroll_area_->verticalScrollBar()->setValue(500);
    });
}

void SettingDialog::paintEvent(QPaintEvent *event)
{
}
