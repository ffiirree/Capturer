#include "graphmenu.h"
#include <QColorDialog>
#include <QDebug>
#include <QPixmap>

GraphMenu::GraphMenu(QWidget * parent)
    : QFrame(parent)
{
    setCursor(Qt::ArrowCursor);

    const int HEIGHT = 30;
    setFixedHeight(HEIGHT);

    layout_ = new QHBoxLayout();
    layout_->setSpacing(0);
    layout_->setMargin(0);
    setLayout(layout_);

    QPushButton * dot_01 = new QPushButton();
    dot_01->setObjectName("rectangle_btn");
    dot_01->setIcon(QIcon(":/icon/res/dot.png"));
    dot_01->setIconSize(QSize(5, 5));
    dot_01->setFixedSize(HEIGHT, HEIGHT);
    connect(dot_01, &QPushButton::clicked, [=](){
        emit setWidth(1);
        this->click(dot_01);
    });
    layout_->addWidget(dot_01);

    QPushButton * dot_02 = new QPushButton();
    dot_02->setObjectName("rectangle_btn");
    dot_02->setIcon(QIcon(":/icon/res/dot.png"));
    dot_02->setIconSize(QSize(10, 10));
    dot_02->setFixedSize(HEIGHT, HEIGHT);
    connect(dot_02, &QPushButton::clicked, [=]() {
        emit setWidth(3);
        this->click(dot_02);
    });
    layout_->addWidget(dot_02);

    QPushButton * dot_03 = new QPushButton();
    dot_03->setObjectName("rectangle_btn");
    dot_03->setIcon(QIcon(":/icon/res/dot.png"));
    dot_03->setIconSize(QSize(15, 15));
    dot_03->setFixedSize(HEIGHT, HEIGHT);
    connect(dot_03, &QPushButton::clicked, [=]() {
        emit setWidth(5);
        this->click(dot_03);
    });
    layout_->addWidget(dot_03);

    QPushButton * fill_btn = new QPushButton();
    fill_btn->setObjectName("rectangle_btn");
    fill_btn->setIcon(QIcon(":/icon/res/rectangle.png"));
    fill_btn->setIconSize(QSize(20, 20));
    fill_btn->setFixedSize(HEIGHT, HEIGHT);
    connect(fill_btn, &QPushButton::clicked, [=]() {
        emit setFill(fill_btn != selected_btn_);
        this->click(fill_btn);
    });
    layout_->addWidget(fill_btn);

    // color button
    auto color_dialog_ = new QColorDialog();
    color_dialog_->setOptions(QColorDialog::DontUseNativeDialog | QColorDialog::ShowAlphaChannel);
    color_dialog_->setWindowFlags(Qt::WindowStaysOnTopHint);
    color_dialog_->setWindowState(Qt::WindowActive);

    color_btn_ = new QPushButton();
    color_btn_->setObjectName("color_btn");

    connect(color_dialog_, &QColorDialog::colorSelected, [&](const QColor&color) {
        if(color.isValid()) {
            emit setColor(color);
            QPixmap icon(25, 25);
            icon.fill(color);
            color_btn_->setIcon(QIcon(icon));
        }
    });

    QPixmap map(25, 25);
    map.fill(Qt::cyan);
    color_btn_->setIcon(QIcon(map));
    color_btn_->setIconSize(QSize(25, 25));
    color_btn_->setFixedSize(HEIGHT, HEIGHT);
    connect(color_btn_, &QPushButton::clicked, [=]() {
        color_dialog_->show();
    });
    layout_->addWidget(color_btn_);

    hide();
}

void GraphMenu::click(QPushButton* btn)
{
    if(selected_btn_ != nullptr) {
        selected_btn_->setProperty("selected", false);
        selected_btn_->style()->unpolish(selected_btn_);
        selected_btn_->style()->polish(selected_btn_);
        selected_btn_->update();
    }

    if(btn != selected_btn_) {
        btn->setProperty("selected", true);
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
        btn->update();
        selected_btn_ = btn;
    }
    else {
        selected_btn_ = nullptr;
    }
}

void GraphMenu::unselectAll()
{
    if(!selected_btn_) return;

    selected_btn_->setProperty("selected", false);
    selected_btn_->style()->unpolish(selected_btn_);
    selected_btn_->style()->polish(selected_btn_);
    selected_btn_->update();

    selected_btn_ = nullptr;
}

