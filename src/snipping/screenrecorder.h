#ifndef CAPTURER_SCREEN_RECORDER_H
#define CAPTURER_SCREEN_RECORDER_H

#include "libcap/consumer.h"
#include "libcap/dispatcher.h"
#include "libcap/screen-capturer.h"
#include "menu/recording-menu.h"
#include "selector.h"

class QTimer;

class ScreenRecorder final : public QWidget
{
    Q_OBJECT

public:
    enum
    {
        VIDEO,
        GIF
    };

public:
    explicit ScreenRecorder(int type = VIDEO, QWidget * = nullptr);

signals:
    void saved(const QString& path);

public slots:
    void start();
    void record();
    void stop();

    void mute(int type, bool v);

    void setStyle(const SelectorStyle& style);

private:
    void keyPressEvent(QKeyEvent *) override;

    void setup();

    int rec_type_{ VIDEO };

    Selector *selector_{};

    bool recording_{ false };

    // parameters
    AVPixelFormat                      pix_fmt_{ AV_PIX_FMT_YUV420P };
    std::string                        codec_name_{ "libx264" };
    AVRational                         framerate_{ 30, 1 };
    std::string                        filters_{};
    std::map<std::string, std::string> encoder_options_{};

    // filename
    std::string filename_{};

    // recording menu
    RecordingMenu *menu_{};
    bool           m_mute_{};
    bool           s_mute_{};

    // sources
    std::unique_ptr<ScreenCapturer>      desktop_src_{};
    std::unique_ptr<Producer<av::frame>> mic_src_{};
    std::unique_ptr<Producer<av::frame>> speaker_src_{};

    // dispatcher
    std::unique_ptr<Dispatcher> dispatcher_{};

    // sink
    std::unique_ptr<Consumer<av::frame>> encoder_{};

    // timer for displaying time on recording menu
    QTimer *timer_{ nullptr };
};

#endif //! CAPTURER_SCREEN_RECORDER_H
