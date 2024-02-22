#ifndef CAPTURER_VIDEO_PLAYER_H
#define CAPTURER_VIDEO_PLAYER_H

#include "control-widget.h"
#include "framelesswindow.h"
#include "libcap/audio-renderer.h"
#include "libcap/consumer.h"
#include "libcap/decoder.h"
#include "libcap/dispatcher.h"
#include "libcap/sonic.h"
#include "libcap/timeline.h"
#include "menu.h"
#include "texture-widget-d3d11.h"
#include "texture-widget-opengl.h"

#include <QTimer>

class VideoPlayer final : public FramelessWindow, public Consumer<av::frame>
{
    Q_OBJECT

    enum : uint8_t
    {
        AINPUT_EOF = 0x01,
        VINPUT_EOF = 0x02,
        APLAY_EOF  = 0x04,
        VPLAY_EOF  = 0x08,
        INPUT_EOF  = AINPUT_EOF | VINPUT_EOF,
        PLAY_EOF   = APLAY_EOF | VPLAY_EOF,
    };

public:
    explicit VideoPlayer(QWidget *parent = nullptr);

    ~VideoPlayer() override;

    int open(const std::string&, std::map<std::string, std::string>) override;

    int start() override;

    void stop() override;

    int consume(const av::frame& frame, AVMediaType type) override;

    void               enable(AVMediaType type, bool v) override;
    [[nodiscard]] bool accepts(AVMediaType type) const override;

    [[nodiscard]] bool eof() const override;

public slots:
    void pause() override;
    void resume() override;

    void seek(std::chrono::nanoseconds, std::chrono::nanoseconds); // us

    void mute(bool);

    void setSpeed(float);

    void setVolume(int);

    void showProperties();

    void finish();

signals:
    void started();
    void timeChanged(int64_t);

    void video_finished();
    void audio_finished();

protected:
    bool event(QEvent *event) override;

    void closeEvent(QCloseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    void contextMenuEvent(QContextMenuEvent *) override;

private:
    void video_thread_fn();

    void initContextMenu();

    std::string filename_{};

    bool is_live_{};

    // UI
    ControlWidget   *control_{};
    TextureGLWidget *texture_{};

    QTimer *timer_{};

    Menu         *menu_{};
    QMenu        *asmenu_{};
    QActionGroup *asgroup_{};
    QMenu        *ssmenu_{};
    QActionGroup *ssgroup_{};

    // video & audio
    std::jthread video_thread_;

    std::unique_ptr<Decoder>       decoder_{};
    std::unique_ptr<AudioRenderer> audio_renderer_{};
    std::unique_ptr<Dispatcher>    dispatcher_{}; // TODO: ctor & dtor order

    std::atomic<bool> video_enabled_{ false };
    std::atomic<bool> audio_enabled_{ false };

    safe_queue<av::frame> vbuffer_{ 4 };
    safe_queue<av::frame> abuffer_{ 16 };
    sonic_stream         *sonic_stream_{}; // audio speed up / down

    std::atomic<bool> seeking_{ false };
    std::atomic<int>  vstep_{ 0 };
    std::atomic<int>  astep_{ 0 };

    // clock & timeline @{
    // | renderer buffered samples |  sonic samples  |  decoding
    //                                               ^
    //                                               |
    //                                           audio pts
    std::atomic<std::chrono::nanoseconds> audio_pts_{ av::clock::nopts };

    av::timeline_t timeline_{};
    // @}
};

#endif // !CAPTURER_VIDEO_PLAYER_H