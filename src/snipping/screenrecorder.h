#ifndef CAPTURER_SCREEN_RECORDER_H
#define CAPTURER_SCREEN_RECORDER_H

#include "libcap/consumer.h"
#include "libcap/dispatcher.h"
#include "libcap/producer.h"
#include "menu/recording-menu.h"
#include "selector.h"
#include "videoplayer.h"

#include <QSystemTrayIcon>
#include <QTimer>

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
    void SHOW_MESSAGE(const QString&, const QString&,
                      QSystemTrayIcon::MessageIcon = QSystemTrayIcon::Information, int = 10'000);

public slots:
    void start();
    void record();
    void stop();

    void switchCamera();

    void mute(int type, bool v);

    void updateTheme();

private:
    void keyPressEvent(QKeyEvent *) override;

    void setup();

    int recording_type_{ VIDEO };

    Selector *selector_{};

    bool recording_{ false };

    // parameters
    AVPixelFormat pix_fmt_{ AV_PIX_FMT_YUV420P };
    std::string codec_name_{ "libx264" };
    std::string quality_{ "medium" };
    int framerate_{ 30 };
    std::string filters_{};
    std::map<std::string, std::string> encoder_options_{};

    // filename
    std::string filename_{};

    // preview camera
    VideoPlayer *player_{};

    // recording menu
    RecordingMenu *menu_{};
    bool m_mute_{};
    bool s_mute_{};

    // sources
    std::unique_ptr<Producer<AVFrame>> desktop_capturer_{};
    std::unique_ptr<Producer<AVFrame>> microphone_capturer_{};
    std::unique_ptr<Producer<AVFrame>> speaker_capturer_{};

    // encoder
    std::unique_ptr<Consumer<AVFrame>> encoder_{};
    std::unique_ptr<Dispatcher> dispatcher_{};

    // timer for displaying time on recording menu
    QTimer *timer_{ nullptr };

    // TODO: wgc bgra -> transparent / black frames.
    // clang-format off
    std::map<std::string, std::string> gif_filters_{
        { "high",   "[0:v] split [a][b];[a] palettegen=stats_mode=single:max_colors=256 [p];[b][p] paletteuse=new=1" },
        { "medium", "[0:v] split [a][b];[a] palettegen=stats_mode=single:max_colors=128 [p];[b][p] paletteuse=new=1:dither=none" },
        { "low",    "[0:v] split [a][b];[a] palettegen=stats_mode=single:max_colors=96  [p];[b][p] paletteuse=new=1:dither=none" },
    };

    std::map<std::string, std::string> video_qualities_ = {
        { "high",   "18" },
        { "medium", "23" },
        { "low",    "28" },
    };
    // clang-format on
};

#endif //! CAPTURER_SCREEN_RECORDER_H
