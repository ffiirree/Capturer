#ifndef RECORD_MENU_H
#define RECORD_MENU_H

#include <QWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>

class RecordMenu : public QWidget
{
    Q_OBJECT

public:
    enum :uint8_t {
        NONE = 0x00,
        MICROPHONE = 0x01,
        SPEAKER = 0x02,
        CAMERA = 0x04,
        PAUSE = 0x08,
        ALL = 0xff
    };

    explicit RecordMenu(bool, bool, uint8_t = ALL, QWidget* parent = nullptr);


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
    void close_camera();

private:
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    QWidget* window_{ nullptr };

    QPushButton* close_btn_{ nullptr };
    QCheckBox* mic_{ nullptr };
    QCheckBox* speaker_{ nullptr };
    QCheckBox* camera_{ nullptr };
    QCheckBox* pause_{ nullptr };
    QLabel* time_label_{ nullptr };

    QPoint begin_pos_{ 0, 0 };
    bool moving_{ false };
};

#endif // RECORD_MENU_H
