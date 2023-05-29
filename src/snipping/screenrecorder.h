#ifndef SCREEN_RECORDER_H
#define SCREEN_RECORDER_H

#include "config.h"
#include "decoder.h"
#include "dispatcher.h"
#include "encoder.h"
#include "menu/recording-menu.h"
#include "selector.h"
#include "videoplayer.h"

#include <QSystemTrayIcon>
#include <QTimer>

#if _WIN32
#include "win-wasapi/wasapi-capturer.h"
#include "win-wgc/wgc-capturer.h"
#endif

#ifdef _WIN32
using DesktopCapturer = WindowsGraphicsCapturer;
using AudioCapturer   = WasapiCapturer;
#elif __linux__
using DesktopCapturer = Decoder;
using AudioCapturer   = Decoder;
#endif

class ScreenRecorder : public Selector
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

    [[nodiscard]] inline int framerate() const { return framerate_; }

signals:
    void SHOW_MESSAGE(const QString&, const QString&,
                      QSystemTrayIcon::MessageIcon = QSystemTrayIcon::Information, int = 10'000);

public slots:
    void start();
    void record();

    void switchCamera();

    void mute(int type, bool v)
    {
        switch (type) {
        case 1:
            m_mute_ = v;
            microphone_capturer_->mute(v);
            break;
        case 2: s_mute_ = v; speaker_capturer_->mute(v);
        default: break;
        }
    }

    void updateTheme()
    {
        const auto& style = Config::instance()[recording_type_ == VIDEO ? "record" : "gif"]["selector"];
        Selector::setBorderStyle(QPen{
            style["border"]["color"].get<QColor>(),
            static_cast<qreal>(style["border"]["width"].get<int>()),
            style["border"]["style"].get<Qt::PenStyle>(),
        });

        Selector::setMaskStyle(style["mask"]["color"].get<QColor>());
    }

private:
    void keyPressEvent(QKeyEvent *) override;
    void hideEvent(QHideEvent *) override;

    void open_audio_sources();
    void setup();

    int recording_type_{ VIDEO };

    int framerate_{ 30 };
    AVPixelFormat pix_fmt_{ AV_PIX_FMT_YUV420P };
    std::string codec_name_{ "libx264" };
    std::string quality_{ "medium" };
    std::string filters_{};
    std::map<std::string, std::string> encoder_options_{};

    std::string filename_;

    VideoPlayer *player_{ nullptr };

    RecordMenu *menu_{ nullptr };
    bool m_mute_{ false };
    bool s_mute_{ false };

    std::unique_ptr<Producer<AVFrame>> desktop_capturer_{};
    std::unique_ptr<Producer<AVFrame>> microphone_capturer_{};
    std::unique_ptr<Producer<AVFrame>> speaker_capturer_{};

    std::unique_ptr<Consumer<AVFrame>> encoder_{};
    std::unique_ptr<Dispatcher> dispatcher_{};

    QTimer *timer_{ nullptr };

    // TODO: wgc bgra -> transparent / black frames.
    std::map<std::string, std::string> gif_filters_{
        { "high",
          "[0:v] split [a][b];[a] palettegen=stats_mode=single:max_colors=256 [p];[b][p] paletteuse=new=1" },
        { "medium",
          "[0:v] split [a][b];[a] palettegen=stats_mode=single:max_colors=128 [p];[b][p] paletteuse=new=1:dither=none" },
        { "low",
          "[0:v] split [a][b];[a] palettegen=stats_mode=single:max_colors=96  [p];[b][p] paletteuse=new=1:dither=none" },
    };

    std::map<std::string, std::string> video_qualities_ = {
        { "high", "18" },
        { "medium", "23" },
        { "low", "28" },
    };
};

#endif // SCREEN_RECORDER_H
