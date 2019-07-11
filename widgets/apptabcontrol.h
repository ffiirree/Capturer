#ifndef APPTABCONTROL_H
#define APPTABCONTROL_H

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
    AppTabControl(QWidget *parent);
    AppTabControl(int width, int height, QWidget *parent = nullptr);

    void setTabControlSize(int width, int height);
	~AppTabControl() = default;

private:
	int width_ = 0;
	int height_ = 0;
};

#endif // APPTABCONTROL_H