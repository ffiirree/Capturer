#include "apptabcontrol.h"

AppTabControl::AppTabControl(QWidget *parent) : QTabWidget(parent)
{
    this->setTabBar(new AppTabBar(width_, height_, parent));
    this->setTabPosition(QTabWidget::West);
}

AppTabControl::AppTabControl(int width, int height, QWidget *parent)
	: QTabWidget(parent), width_(width), height_(height)
{
    this->setTabBar(new AppTabBar(width_, height_, parent));
    this->setTabPosition(QTabWidget::West);
}

void AppTabControl::setTabControlSize(int width, int height)
{
    width_ = width;
    height_ = height;
}