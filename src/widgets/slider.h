#ifndef CAPTURER_SLIDER_H
#define CAPTURER_SLIDER_H

#include "libcap/clock.h"

#include <QSlider>

class Slider final : public QSlider
{
    Q_OBJECT
public:
    explicit Slider(QWidget *parent = nullptr);
    explicit Slider(Qt::Orientation orientation, QWidget *parent = nullptr);

signals:
    void seek(std::chrono::nanoseconds, std::chrono::nanoseconds); // AV_TIME_BASE

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
};

#endif //! CAPTURER_SLIDER_H