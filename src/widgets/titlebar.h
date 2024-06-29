#ifndef CAPTURER_TITLE_BAR_H
#define CAPTURER_TITLE_BAR_H

#include "framelesswindow.h"

#include <QPointer>

class QAbstractButton;
class QLabel;

class TitleBar final : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(FramelessWindow *parent);

    void setHideOnFullScreen(bool value = true) { hide_on_fullscreen_ = value; }

    [[nodiscard]] bool isInSystemButtons(const QPoint& pos) const;

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    FramelessWindow *window_{};
    bool             hide_on_fullscreen_{ true };

    QPointer<QAbstractButton> icon_btn_{};
    QPointer<QLabel>          title_label_{};
    QPointer<QAbstractButton> pin_btn_{};
    QPointer<QAbstractButton> min_btn_{};
    QPointer<QAbstractButton> max_btn_{};
    QPointer<QAbstractButton> full_btn_{};
    QPointer<QAbstractButton> close_btn_{};
};
#endif //! CAPTURER_TITLE_BAR_H
