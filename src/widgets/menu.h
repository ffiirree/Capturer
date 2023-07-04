#ifndef CAPTURER_MENU_H
#define CAPTURER_MENU_H

#include <QMenu>

class Menu : public QMenu
{
    Q_OBJECT
public:
    explicit Menu(QWidget *parent = nullptr);
    explicit Menu(const QString &title, QWidget *parent = nullptr);
};

#endif