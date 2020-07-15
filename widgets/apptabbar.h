#ifndef APPTABBAR_H
#define APPTABBAR_H

#include <QTabBar>
#include <QStylePainter>
#include <QStyleOptionTab>
#include <QProxyStyle>

class AppTabBar : public QTabBar
{
public:
	AppTabBar(QWidget *parent = nullptr) 
		: QTabBar(parent) 
	{
		setCursor(Qt::PointingHandCursor);
	}
    AppTabBar(int tab_width, int tab_height, QWidget *parent = nullptr)
		: QTabBar(parent), width_(tab_width), height_(tab_height) 
	{
		setCursor(Qt::PointingHandCursor);
	}

    QSize tabSizeHint(int index) const override;
    ~AppTabBar() override = default;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
	int width_ = 30;
	int height_ = 115;
};

#endif // APPTABBAR_H
