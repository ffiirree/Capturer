#ifndef CAPTURER_VIDEO_PLAYER_H
#define CAPTURER_VIDEO_PLAYER_H

#include "control-widget.h"
#include "framelesswindow.h"
#include "libcap/audio-fifo.h"
#include "libcap/audio-renderer.h"
#include "libcap/consumer.h"
#include "libcap/decoder.h"
#include "libcap/dispatcher.h"
#include "libcap/timeline.h"
#include "menu.h"
#include "texture-widget-d3d11.h"
#include "texture-widget-opengl.h"
#include "texture-widget.h"

#include <QTimer>

class VideoPlayer final : public FramelessWindow, public Consumer<AVFrame>
{
    Q_OBJECT

public:
    explicit VideoPlayer(QWidget *parent = nullptr);

    ~VideoPlayer() override;

    int open(const std::string&, std::map<std::string, std::string>) override;

    int run() override;

    [[nodiscard]] bool ready() const override
    {
        return decoder_ && decoder_->ready() && audio_renderer_ &&
               ((audio_renderer_->ready() && audio_enabled_) || !audio_enabled_);
    }

    void stop() override
    {
        if (dispatcher_) dispatcher_->reset();
    }

    void reset() override;

    [[nodiscard]] bool full(AVMediaType type) const override;
    int                consume(AVFrame *frame, AVMediaType type) override;

    void               enable(AVMediaType type, bool v = true) override;
    [[nodiscard]] bool accepts(AVMediaType type) const override;

    [[nodiscard]] bool eof() const override
    {
        return (((eof_ & 0x01) && video_enabled_) || !video_enabled_) &&
               (((eof_ & 0x02) && audio_enabled_) || !audio_enabled_);
    }

    bool paused() const { return paused_; }

public slots:
    void pause();
    void resume();
    void seek(std::chrono::nanoseconds, std::chrono::nanoseconds); // us
    void mute(bool);
    void setSpeed(float);
    void setVolume(int);
    void showPreferences();
    void showProperties();

signals:
    void started();
    void timeChanged(std::chrono::nanoseconds);

protected:
    void closeEvent(QCloseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    void contextMenuEvent(QContextMenuEvent *) override;

    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void video_thread_f();

    void initContextMenu();

    // UI
    TextureGLWidget *texture_{};

    ControlWidget *control_{};
    QTimer        *timer_{};

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

    safe_queue<av::frame>            vbuffer_{};
    std::unique_ptr<safe_audio_fifo> abuffer_{};

    std::atomic<bool> seeking_{ false };
    std::atomic<int>  step_{ 0 };

    std::atomic<bool> paused_{ false };

    // clock & timeline @{
    // | audio renderer buffered samples |   abuffer samples  |  decoding
    //                                                        ^
    //                                                        |
    //                                                    audio pts
    std::atomic<std::chrono::nanoseconds> audio_pts_{ av::clock::nopts };

    av::timeline_t timeline_{};
    // @}
};

#endif // !CAPTURER_VIDEO_PLAYER_H