#ifndef CAPTURER_NAVIGATION_BAR_H
#define CAPTURER_NAVIGATION_BAR_H

#include <QWidget>

class QButtonGroup;
class QAbstractButton;

class NavigationBar final : public QWidget
{
    Q_OBJECT

public:
    explicit NavigationBar(QWidget *parent = nullptr);

    void add(QAbstractButton *button, int id = -1);

    [[nodiscard]] int id() const;
    void              setId(int id);

signals:
    void toggled(int);

private:
    QButtonGroup *group_{};
};

#endif //! CAPTURER_NAVIGATION_BAR_H