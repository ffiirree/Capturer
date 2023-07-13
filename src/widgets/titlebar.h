#ifndef CAPTURER_TITLE_BAR_H
#define CAPTURER_TITLE_BAR_H

#include "framelesswindow.h"

class TitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(FramelessWindow *parent);

    FramelessWindow *window() const { return window_; }

    void setHideOnFullScreen(bool value = true) { hide_on_fullscreen_ = value; }

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    FramelessWindow *window_{};
    bool hide_on_fullscreen_{ true };
#ifndef Q_OS_WIN
    QPoint begin_{ 0, 0 };
    bool moving_{ false };
#endif
};
#endif //! CAPTURER_TITLE_BAR_H
