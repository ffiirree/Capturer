#ifndef CAPTURER_NAVIGATION_BAR_H
#define CAPTURER_NAVIGATION_BAR_H

#include <QAbstractButton>
#include <QButtonGroup>

class NavigationBar : public QWidget
{
    Q_OBJECT

public:
    explicit NavigationBar(QWidget *parent = nullptr);

    void add(QAbstractButton *button, int id = -1);

    int  id() const;
    void setId(int id);

signals:
    void toggled(int);

private:
    QButtonGroup *group_{};
};

#endif //! CAPTURER_NAVIGATION_BAR_H