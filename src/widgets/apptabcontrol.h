#ifndef APP_TAB_CONTROL_H
#define APP_TAB_CONTROL_H

#include <QTabWidget>
#include <QTabBar>
#include <QPalette>
#include <QPainter>
#include <QDebug>
#include "apptabbar.h"

class AppTabControl : public QTabWidget
{
public:
    AppTabControl() = default;
    explicit AppTabControl(QWidget *parent);
    AppTabControl(int width, int height, QWidget *parent = nullptr);

    void setTabControlSize(int width, int height);
    ~AppTabControl() override = default;

private:
    int width_ = 0;
    int height_ = 0;
};

#endif // APP_TAB_CONTROL_H