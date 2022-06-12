#include "apptabcontrol.h"

AppTabControl::AppTabControl(QWidget *parent) : QTabWidget(parent)
{
    setTabBar(new AppTabBar(width_, height_, parent));
    setTabPosition(QTabWidget::West);
}

AppTabControl::AppTabControl(int width, int height, QWidget *parent)
    : QTabWidget(parent), width_(width), height_(height)
{
    setTabBar(new AppTabBar(width_, height_, parent));
    setTabPosition(QTabWidget::West);
}

void AppTabControl::setTabControlSize(int width, int height)
{
    width_ = width;
    height_ = height;
}