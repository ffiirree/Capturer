#ifndef CAPTURER_CONTROL_WIDGET_H
#define CAPTURER_CONTROL_WIDGET_H

#include <QCheckBox>
#include <QLabel>
#include <QSlider>
#include <QWidget>

class ControlWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ControlWidget(QWidget *parent);

public slots:
    void setDuration(int64_t);
    void setTime(int64_t);

    bool paused() const;

signals:
    void pause();
    void resume();

private:
    QSlider *time_slider_{};
    QSlider *volume_slider_{};

    QLabel *time_label_{};
    QLabel *duration_label_{};

    QCheckBox *pause_btn_{};
};

#endif //! CAPTURER_CONTROL_WIDGET_H