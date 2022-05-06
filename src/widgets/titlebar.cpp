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
	icon_label_ = new QLabel();
    icon_label_->setObjectName("logo");
    auto logo = QPixmap(":/icon/res/icon");
    icon_label_->setPixmap(logo);
    icon_label_->setFixedSize(50, 50);
    icon_label_->setContentsMargins({10, 10, 10, 10});
    icon_label_->setScaledContents(true);

	title_label_ = new QLabel("");
    title_label_->setFixedWidth(175);
	title_label_->setObjectName("title");

    layout->addWidget(icon_label_);
    layout->addWidget(title_label_);

    // blank
	layout->addSpacerItem(new QSpacerItem(20, 50, QSizePolicy::Expanding, QSizePolicy::Minimum));

    // close button
    close_btn_ = new IconButton(QPixmap(":/icon/res/close"), { 50, 50 }, { 24, 24 });
    close_btn_->normal(QColor("#afafaf"));
    close_btn_->hover(QColor("#409eff"));
	connect(close_btn_, &QPushButton::clicked, this, &TitleBar::close);

	layout->addWidget(close_btn_);
}

void TitleBar::mousePressEvent(QMouseEvent * event)
{
	if (event->button() == Qt::LeftButton) {
        begin_ = event->globalPos();
		moving = true;
	}
}

void TitleBar::mouseMoveEvent(QMouseEvent * event)
{
    auto mouse_global_pos = event->globalPos();
    if (moving) {
        moved(mouse_global_pos - begin_);
        begin_ = mouse_global_pos;
    }
}

void TitleBar::mouseReleaseEvent(QMouseEvent *)
{
    moving = false;
}
