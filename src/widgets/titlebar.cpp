#include "titlebar.h"
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QStyle>
#include <QPushButton>

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground);
    setAutoFillBackground(true);

    auto layout = new QHBoxLayout();
    layout->setMargin(0);
    layout->setSpacing(0);
    setLayout(layout);

    // logo & title
    auto icon_label = new QLabel(this);
    icon_label->setObjectName("logo");
    auto logo = QPixmap(":/icon/res/icon");
    icon_label->setPixmap(logo);
    icon_label->setFixedSize(50, 50);
    icon_label->setContentsMargins({10, 10, 10, 10});
    icon_label->setScaledContents(true);

    title_label_ = new QLabel("", this);
    title_label_->setFixedWidth(150);
    title_label_->setObjectName("title");

    layout->addWidget(icon_label);
    layout->addWidget(title_label_);

    // blank
    layout->addSpacerItem(new QSpacerItem(20, 50, QSizePolicy::Expanding, QSizePolicy::Minimum));

    // close button
    auto close_btn = new IconButton(QPixmap(":/icon/res/close"), { 50, 50 }, { 24, 24 }, false, this);
    close_btn->normal(QColor("#afafaf"));
    close_btn->hover(QColor("#409eff"));
    connect(close_btn, &QPushButton::clicked, this, &TitleBar::close);

    layout->addWidget(close_btn);
}

void TitleBar::mousePressEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton) {
        begin_ = event->globalPos();
        moving_ = true;
    }
}

void TitleBar::mouseMoveEvent(QMouseEvent * event)
{
    auto mouse_global_pos = event->globalPos();
    if (moving_) {
        moved(mouse_global_pos - begin_);
        begin_ = mouse_global_pos;
    }
}

void TitleBar::mouseReleaseEvent(QMouseEvent *)
{
    moving_ = false;
}
