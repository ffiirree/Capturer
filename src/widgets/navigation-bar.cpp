#include "navigation-bar.h"

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

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(group_, &QButtonGroup::idToggled, [this](int id, bool tged) {
#else
    connect(group_, QOverload<int, bool>::of(&QButtonGroup::buttonToggled), [this](int id, bool tged) {
#endif
        if (tged) emit toggled(id);
    });
}

void NavigationBar::add(QAbstractButton *button, int id)
{
    group_->addButton(button, id);
    layout()->addWidget(button);
}

int NavigationBar::id() const { return group_->checkedId(); }

void NavigationBar::setId(int id) { group_->button(id)->setChecked(true); }