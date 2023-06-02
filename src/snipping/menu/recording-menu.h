#ifndef CAPTURER_RECORD_MENU_H
#define CAPTURER_RECORD_MENU_H

#include <QCheckBox>
#include <QLabel>

class RecordMenu : public QWidget
{
    Q_OBJECT

public:
    enum : uint8_t
    {
        NONE       = 0x00,
        MICROPHONE = 0x01,
        SPEAKER    = 0x02,
        CAMERA     = 0x04,
        PAUSE      = 0x08,
        ALL        = 0xff
    };

    explicit RecordMenu(bool, bool, uint8_t = ALL, QWidget *parent = nullptr);

signals:
    void started();
    void paused();
    void resumed();
    void muted(int, bool);
    void stopped();
    void opened(bool);

public slots:
    void start();
    void time(int64_t);
    void mute(int, bool);
    void camera_checked(bool);

    void disable_mic(bool);
    void disable_cam(bool);
    void disable_speaker(bool);

private:
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    QWidget *window_{ nullptr };

    QCheckBox *mic_btn_{ nullptr };
    QCheckBox *speaker_btn_{ nullptr };
    QCheckBox *camera_btn_{ nullptr };
    QCheckBox *pause_btn_{ nullptr };
    QCheckBox *close_btn_{ nullptr };

    QLabel *time_label_{ nullptr };

    QPoint begin_pos_{ 0, 0 };
    bool moving_{ false };
};

#endif //! CAPTURER_RECORD_MENU_H
