#ifndef CAPTURER_CONTROL_WIDGET_H
#define CAPTURER_CONTROL_WIDGET_H

#include "framelesswindow.h"
#include "slider.h"

#include <QCheckBox>
#include <QLabel>

class ControlWidget final : public QWidget
{
    Q_OBJECT
public:
    explicit ControlWidget(FramelessWindow *parent);

public slots:
    void setDuration(int64_t duration); // milliseconds
    void setTime(int64_t ts);           // milliseconds

    void setVolume(int);
    void setMute(bool);

    [[nodiscard]] bool paused() const;

signals:
    void pause();
    void resume();
    void seek(std::chrono::nanoseconds ts, std::chrono::nanoseconds rel);
    void speed(float);
    void volume(int);
    void mute(bool);

    void validDruation(bool);

private:
    Slider    *time_slider_{};
    QCheckBox *volume_btn_{};
    Slider    *volume_slider_{};

    QLabel *time_label_{};
    QLabel *duration_label_{};

    QCheckBox *pause_btn_{};
};

#endif //! CAPTURER_CONTROL_WIDGET_H