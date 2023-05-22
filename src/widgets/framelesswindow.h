#ifndef CAPTURER_FRAMELESS_WINDOW_H
#define CAPTURER_FRAMELESS_WINDOW_H

#include <QWidget>

class QGraphicsDropShadowEffect;

class FramelessWindow : public QWidget
{
public:
    explicit FramelessWindow(QWidget *parent = nullptr);

    void setWidget(QWidget *);

protected:
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

    QGraphicsDropShadowEffect *effect_{ nullptr };

    QPoint moving_begin_{ -1, -1 };
    QPoint resizing_begin_{ -1, -1 };

    uint32_t hover_flags_{ 0x00 };
    bool is_resizing_{ false };
    bool is_moving_{ false };
};

#endif // !CAPTURER_FRAMELESS_WINDOW_H
