#ifndef BUTTONGROUP_H
#define BUTTONGROUP_H

#include <QAbstractButton>
#include <QHash>

enum class exclusive_t
{
    exclusive,     // exclusive and cannot uncheck the currently checked button by clicking on it
    simiexclusive, // exclusive but can uncheck the currently checked button by clicking on it
    nonexclusive,
};

class ButtonGroup : public QObject // inherit from QObject, since the behaivor of QButtonGroup is bound with
                                   // QAbstractButton
{
    Q_OBJECT
public:
    explicit ButtonGroup(QObject *parent = nullptr)
        : QObject(parent)
    {}

    void addButton(QAbstractButton *button, int id = -1);

    // may be nullptr
    QAbstractButton *checkedButton() const { return checked_; }

    // return -1 if not found
    int checkedId() const { return mapping_.value(checked_, -1); }

    // retrieve button pointer by id, return nullptr if not found
    QAbstractButton *button(int id) const { return mapping_.key(id, nullptr); }

    // retrieve id by button pointer, return -1 if not found
    int id(QAbstractButton *button) const { return mapping_.value(button, -1); }

    //
    QList<QAbstractButton *> buttons() const { return buttons_; }

    // exclusive
    exclusive_t exclusive() const { return exclusive_; };

    void setExclusive(exclusive_t exclusive) { exclusive_ = exclusive; }

public slots:
    // unchecked all buttons if not exclusive
    void clear();

signals:
    void buttonClicked(QAbstractButton *button);
    void buttonToggled(QAbstractButton *button, bool checked);

    void idClicked(int id);
    void idToggled(int id, bool checked);

    void emptied();

private:
    void ontoggled(QAbstractButton *, bool);

    QList<QAbstractButton *> buttons_{};
    QHash<QAbstractButton *, int> mapping_{};

    QAbstractButton *checked_{};
    exclusive_t exclusive_{ exclusive_t::simiexclusive };
};

#endif // BUTTONGROUP_H
