#include "sizeinfo.h"

SizeInfoWidget::SizeInfoWidget(QWidget *parent)
    : QWidget(parent)
{
    label_ = new QLabel(this);
    label_->setFixedSize(100, 24);

    label_->setAlignment(Qt::AlignCenter);

    QFont font;
    font.setPointSize(10);
    font.setFamily("Consolas");
    label_->setFont(font);

    QPalette p;
    p.setColor(QPalette::WindowText, Qt::white);
    label_->setPalette(p);
}

void SizeInfoWidget::size(const QSize &size)
{
    auto text  = QString::number(size.width()) + " x " + QString::number(size.height());
    label_->setText(text);
}

void SizeInfoWidget::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    painter_.begin(this);
    painter_.fillRect(rect(), QColor(0, 0, 0, 200));
    painter_.end();
}
