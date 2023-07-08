#ifndef CAPTURER_TITLE_BAR_H
#define CAPTURER_TITLE_BAR_H

#include "framelesswindow.h"

class TitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(FramelessWindow *parent);

    FramelessWindow *parent() const { return dynamic_cast<FramelessWindow *>(QWidget::parent()); }

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
#ifndef Q_OS_WIN
    QPoint begin_{ 0, 0 };
    bool moving_{ false };
#endif
};
#endif //! CAPTURER_TITLE_BAR_H
