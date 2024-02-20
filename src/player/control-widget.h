#ifndef CAPTURER_CONTROL_WIDGET_H
#define CAPTURER_CONTROL_WIDGET_H

#include "combobox.h"
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
    void setDuration(int64_t duration); // microseconds
    void setTime(int64_t ts);           // microseconds

    void setVolume(int);
    void setMute(bool);

    void setLiveMode(bool);

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
    ComboBox  *speed_box_{};
    QCheckBox *volume_btn_{};
    Slider    *volume_slider_{};

    QLabel *time_label_{};
    QLabel *duration_label_{};

    QCheckBox *pause_btn_{};
};

#endif //! CAPTURER_CONTROL_WIDGET_H