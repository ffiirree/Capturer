#ifndef BUTTONGROUP_H
#define BUTTONGROUP_H

#include <QAbstractButton>

class ButtonGroup: public QObject
{
    Q_OBJECT
public:
    explicit ButtonGroup(QObject *parent = nullptr)
        : QObject(parent)
    { }

    void addButton(QAbstractButton *btn);
    void uncheckAll();
    QList<QAbstractButton*> buttons() const { return buttons_; }
    QAbstractButton * checkedButton() const { return checked_; }

signals:
    void buttonClicked(QAbstractButton *);
    void buttonClicked(int);
    void buttonToggled(QAbstractButton *, bool);
    void uncheckedAll();

private:
    QList<QAbstractButton *> buttons_;
    QAbstractButton *checked_ = nullptr;
};

#endif // BUTTONGROUP_H
