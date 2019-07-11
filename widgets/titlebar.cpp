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

	setContentsMargins(15, 0, 10, 0);

	auto layout = new QHBoxLayout();
	layout->setSpacing(0);
	layout->setMargin(5);
	setLayout(layout);

	icon_label_ = new QLabel();
	title_label_ = new QLabel("");
	title_label_->setObjectName("title");

	layout->addWidget(icon_label_);			// logo
	layout->addWidget(title_label_);		// title

	layout->addSpacerItem(new QSpacerItem(20, 50, QSizePolicy::Expanding, QSizePolicy::Minimum));

    close_btn_ = new IconButton(QPixmap(":/icon/res/close"), { 30, 30 }, {16, 16 });
    close_btn_->setIconColor(QColor("#d0d0d0"));
    close_btn_->setIconHoverColor(QColor("#ffffff"));
	connect(close_btn_, &QPushButton::clicked, this, &TitleBar::close);

	layout->addWidget(close_btn_);

	// 
    auto logo = QPixmap(":/icon/res/icon");
	icon_label_->setPixmap(logo);
	icon_label_->setFixedSize(30, 30);
	icon_label_->setScaledContents(true);
}

void TitleBar::mousePressEvent(QMouseEvent * event)
{
	if (event->button() == Qt::LeftButton) {
		begin_ = event->pos();
		moving = true;
	}
}

void TitleBar::mouseMoveEvent(QMouseEvent * event)
{
	if (moving)
		moved(event->pos() - begin_);
}

void TitleBar::mouseReleaseEvent(QMouseEvent * event)
{
	moving = false;
}
