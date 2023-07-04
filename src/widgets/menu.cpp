#include "menu.h"

Menu::Menu(QWidget *parent)
    : Menu({}, parent)
{}

Menu::Menu(const QString& title, QWidget *parent)
    : QMenu(title, parent)
{
    setWindowFlag(Qt::FramelessWindowHint);
    setWindowFlag(Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
}