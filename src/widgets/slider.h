#ifndef CAPTURER_SLIDER_H
#define CAPTURER_SLIDER_H

#include <QSlider>

class Slider : public QSlider
{
    Q_OBJECT
public:
    explicit Slider(QWidget *parent = nullptr);
    explicit Slider(Qt::Orientation orientation, QWidget *parent = nullptr);

signals:
    void seek(int64_t, int64_t); // AV_TIME_BASE

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
};

#endif //! CAPTURER_SLIDER_H