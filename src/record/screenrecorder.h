#ifndef SCREEN_RECORDER_H
#define SCREEN_RECORDER_H

#include <QSystemTrayIcon>
#include <QTimer>
#include "selector.h"
#include "record_menu.h"
#include "config.h"
#include "mediadecoder.h"
#include "mediaencoder.h"
#include "videoplayer.h"

class ScreenRecorder : public Selector
{
    Q_OBJECT

public:
    enum { VIDEO, GIF };

public:
    explicit ScreenRecorder(int type = VIDEO, QWidget *parent = nullptr);
    ~ScreenRecorder() override {
        delete decoder_; decoder_= nullptr;
        delete encoder_; encoder_ = nullptr;
    }

    inline int framerate() const { return framerate_; }

signals:
    void SHOW_MESSAGE(const QString &title, const QString &msg,
                      QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information, int msecs = 10000);
public slots:
    virtual void exit() override;

    void start() override;
    void record();
    void setFramerate(int fr) { framerate_ = fr; }

    void mute(int type, bool v) { type ? m_mute_ = v : s_mute_ = v; }
    void updateTheme()
    {
        Selector::updateTheme(recording_type_ == VIDEO ? Config::instance()["record"]["selector"] : Config::instance()["gif"]["selector"]);
    }
private:
    void setup();

    void keyPressEvent(QKeyEvent *event) override;

    int recording_type_{ VIDEO };

    int framerate_{ 30 };
    AVPixelFormat pix_fmt_{ AV_PIX_FMT_YUV420P };
    string codec_name_{ "libx264" };
    string quality_{ "medium" };
    string filters_{};
    map<string, string> options_{};

    string filename_;

    VideoPlayer* player_{ nullptr };

    RecordMenu *menu_{ nullptr };
    bool m_mute_{ true };
    bool s_mute_{ true };

    MediaDecoder* decoder_{ nullptr };
    MediaEncoder* encoder_{ nullptr };
    QThread* decoder_thread_{ nullptr };
    QThread* encoder_thread_{ nullptr };

    QTimer* timer_{ nullptr };

    map<string, string> gif_filters_{
        {"high",        "[0:v] split [a][b];[a] palettegen=stats_mode=single:max_colors=256 [p];[b][p] paletteuse=new=1"},
        {"medium",      "[0:v] split [a][b];[a] palettegen=stats_mode=single:max_colors=128 [p];[b][p] paletteuse=new=1:dither=none"},
        {"low",         "[0:v] split [a][b];[a] palettegen=stats_mode=single:max_colors=64 [p];[b][p] paletteuse=new=1:dither=none"},
    };

    map<string, string> video_qualities_ = {
        {"high",        "18"},
        {"medium",      "23"},
        {"low",         "28"},
    };
};

#endif // SCREEN_RECORDER_H
