#ifndef APP_TAB_BAR_H
#define APP_TAB_BAR_H

#include <QTabBar>

class AppTabBar : public QTabBar
{
public:
	explicit AppTabBar(QWidget *parent = nullptr)
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

#endif // APP_TAB_BAR_H
