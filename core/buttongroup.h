#ifndef BUTTONGROUP_H
#define BUTTONGROUP_H

#include <QAbstractButton>
#include <QPainter>

class CustomButton : public QAbstractButton
{
public:
    CustomButton(QWidget *parent = nullptr);
    CustomButton(const QSize&, bool checkable = false, QWidget *parent = nullptr);

    virtual void paint(QPainter* painter) = 0;

protected:
    virtual void paintEvent(QPaintEvent *) override;
    virtual void enterEvent(QEvent *event) override;
    virtual void leaveEvent(QEvent *event) override;

protected:
    QPainter painter_;

    QColor icon_color_ = Qt::black;
    QColor background_ = Qt::white;
    QColor normal_color_ = Qt::white;
    QColor hover_color_{0xcc, 0xcc, 0xcc};
    QColor checked_color_{0x20, 0x80, 0xF0};
};

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
