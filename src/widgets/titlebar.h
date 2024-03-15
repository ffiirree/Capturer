#ifndef CAPTURER_TITLE_BAR_H
#define CAPTURER_TITLE_BAR_H

#include "framelesswindow.h"

class TitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(FramelessWindow *parent);

    void setHideOnFullScreen(bool value = true) { hide_on_fullscreen_ = value; }

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    FramelessWindow *window_{};
    bool             hide_on_fullscreen_{ true };
    int              dragmove_status_{};
};
#endif //! CAPTURER_TITLE_BAR_H
