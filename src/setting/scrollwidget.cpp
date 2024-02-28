#include "scrollwidget.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QVBoxLayout>

ScrollWidget::ScrollWidget(QWidget *parent)
    : QScrollArea(parent)
{
    setWidgetResizable(true);

    widget_ = new QWidget();
    widget_->setObjectName("scroll-content");

    layout_ = new QVBoxLayout();
    layout_->setContentsMargins({ 15, 0, 15, 30 });
    layout_->setSpacing(30);

    widget_->setLayout(layout_);

    setWidget(widget_);
}

QFormLayout *ScrollWidget::addForm(const QString& label)
{
    const auto box  = new QGroupBox(label);
    const auto form = new QFormLayout();

    box->setLayout(form);
    layout_->addWidget(box);

    return form;
}

void ScrollWidget::addSpacer() { layout_->addStretch(); }
