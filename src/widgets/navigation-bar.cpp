#include "navigation-bar.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QHBoxLayout>

NavigationBar::NavigationBar(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground);

    auto layout = new QHBoxLayout();
    layout->setSpacing(0);
    layout->setContentsMargins({});
    setLayout(layout);

    group_ = new QButtonGroup(this);

    connect(group_, &QButtonGroup::idToggled, [this](int id, bool tged) {
        if (tged) emit toggled(id);
    });
}

void NavigationBar::add(QAbstractButton *button, const int id)
{
    group_->addButton(button, id);
    layout()->addWidget(button);
}

int NavigationBar::id() const { return group_->checkedId(); }

void NavigationBar::setId(const int id) { group_->button(id)->setChecked(true); }