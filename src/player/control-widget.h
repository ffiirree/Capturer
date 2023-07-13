#ifndef CAPTURER_CONTROL_WIDGET_H
#define CAPTURER_CONTROL_WIDGET_H

#include "slider.h"
#include "framelesswindow.h"

#include <QCheckBox>
#include <QLabel>

class ControlWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ControlWidget(FramelessWindow *parent);

public slots:
    void setDuration(int64_t); // AV_TIME_BASE
    void setTime(int64_t);     // AV_TIME_BASE
    void setVolume(int);

    bool paused() const;

signals:
    void pause();
    void resume();
    void seek(int64_t, int64_t); // us
    void speed(float);
    void volume(int);
    void mute(bool);

    void validDruation(bool);

private:
    Slider *time_slider_{};
    QCheckBox *volume_btn_{};
    Slider *volume_slider_{};

    QLabel *time_label_{};
    QLabel *duration_label_{};

    QCheckBox *pause_btn_{};
};

#endif //! CAPTURER_CONTROL_WIDGET_H