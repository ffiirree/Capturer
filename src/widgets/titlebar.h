#ifndef CAPTURER_TITLE_BAR_H
#define CAPTURER_TITLE_BAR_H

#include "framelesswindow.h"

class TitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(FramelessWindow *parent);

    [[nodiscard]] FramelessWindow *window() const { return window_; }

    void setHideOnFullScreen(bool value = true) { hide_on_fullscreen_ = value; }

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    FramelessWindow *window_{};
    bool hide_on_fullscreen_{ true };
};
#endif //! CAPTURER_TITLE_BAR_H
