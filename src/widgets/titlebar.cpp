#include "titlebar.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QStyle>

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
    icon_ = new QCheckBox();
    icon_->setObjectName("logo");
    layout->addWidget(icon_);

    // blank
    layout->addSpacerItem(new QSpacerItem(10, 10, QSizePolicy::Expanding, QSizePolicy::Minimum));

    // close button
    auto close_btn = new QCheckBox(this);
    close_btn->setObjectName("close-btn");
    close_btn->setCheckable(false);

    layout->addWidget(close_btn, 0, Qt::AlignTop);

    // title
    connect(parent, &QWidget::windowTitleChanged, [this](const auto& title) { icon_->setText(title); });
    connect(close_btn, &QCheckBox::clicked, parent, &QWidget::close);
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        begin_  = event->globalPos() - dynamic_cast<QWidget *>(parent())->pos();
        moving_ = true;
        setCursor(Qt::SizeAllCursor);
    }
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if((event->buttons() & Qt::LeftButton) && moving_) {
        dynamic_cast<QWidget *>(parent())->move(event->globalPos() - begin_);
    }
}

void TitleBar::mouseReleaseEvent(QMouseEvent *)
{
    if (moving_) {
        moving_ = false;
        setCursor(Qt::ArrowCursor);
    }
}
