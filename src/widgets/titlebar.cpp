#include "titlebar.h"
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QStyle>
#include <QCheckBox>

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground);
    setAutoFillBackground(true);

    auto layout = new QHBoxLayout();
    layout->setContentsMargins({});
    layout->setSpacing(0);
    setLayout(layout);

    // logo & title
    logo_ = new QCheckBox();
    logo_->setObjectName("logo");
    layout->addWidget(logo_);

    // blank
    layout->addSpacerItem(new QSpacerItem(10, 10, QSizePolicy::Expanding, QSizePolicy::Minimum));

    // close button
    auto close_btn = new QCheckBox(this);
    close_btn->setObjectName("close-btn");
    close_btn->setCheckable(false);
    connect(close_btn, &QCheckBox::clicked, this, &TitleBar::close);

    layout->addWidget(close_btn, 0, Qt::AlignTop);
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
