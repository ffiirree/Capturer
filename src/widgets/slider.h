#ifndef CAPTURER_SLIDER_H
#define CAPTURER_SLIDER_H

#include <chrono>
#include <QSlider>

class Slider final : public QSlider
{
    Q_OBJECT
public:
    explicit Slider(QWidget *parent = nullptr);
    explicit Slider(Qt::Orientation orientation, QWidget *parent = nullptr);

signals:
    void seek(std::chrono::nanoseconds ts, std::chrono::nanoseconds rel);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
};

#endif //! CAPTURER_SLIDER_H