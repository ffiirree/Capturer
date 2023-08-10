#ifndef CAPTURER_VIDEO_PLAYER_H
#define CAPTURER_VIDEO_PLAYER_H

#include "control-widget.h"
#include "framelesswindow.h"
#include "menu.h"
#include "texture-widget-d3d11.h"
#include "texture-widget-opengl.h"
#include "texture-widget.h"

#include <libcap/audio-render.h>
#include <libcap/consumer.h>
#include <libcap/decoder.h>
#include <libcap/dispatcher.h>
#include <QTimer>

extern "C" {
#include <libavutil/audio_fifo.h>
}

class VideoPlayer : public FramelessWindow, public Consumer<AVFrame>
{
    Q_OBJECT

    enum avsync_t
    {
        AUDIO_MASTER   = 0x00,
        VIDEO_MASTER   = 0x01,
        EXTERNAL_CLOCK = 0x02,
    };

public:
    explicit VideoPlayer(QWidget *parent = nullptr);
    ~VideoPlayer();

    int open(const std::string&, std::map<std::string, std::string>) override;

    int run() override;

    [[nodiscard]] bool ready() const override
    {
        return decoder_ && decoder_->ready() && audio_render_ &&
               ((audio_render_->ready() && audio_enabled_) || !audio_enabled_);
    }

    void stop() override
    {
        if (dispatcher_) dispatcher_->reset();
    }

    void reset() override;

    [[nodiscard]] bool full(AVMediaType type) const override;
    int consume(AVFrame *frame, AVMediaType type) override;

    void enable(AVMediaType type, bool v = true) override;
    [[nodiscard]] bool accepts(AVMediaType type) const override;

    [[nodiscard]] bool eof() const override
    {
        return (((eof_ & 0x01) && video_enabled_) || !video_enabled_) &&
               (((eof_ & 0x02) && audio_enabled_) || !audio_enabled_);
    }

    avsync_t sync_type()
    {
        if (avsync_ == avsync_t::AUDIO_MASTER && audio_enabled_) return avsync_t::AUDIO_MASTER;

        return avsync_t::EXTERNAL_CLOCK;
    }

    int64_t clock_ns()
    {
        if (paused_) return clock_.pts;

        return clock_.pts + (os_gettime_ns() - clock_.updated) * clock_.speed;
    }

    float get_speed() { return clock_.speed; }

    void set_clock(int64_t pts, int64_t base_time = AV_NOPTS_VALUE)
    {
        auto systime = (base_time == AV_NOPTS_VALUE) ? os_gettime_ns() : base_time;

        clock_.pts     = pts;
        clock_.updated = systime;
    }

    void set_speed(float speed)
    {
        set_clock(clock_ns());
        clock_.speed = speed;
    }

    bool paused() const { return paused_; }

public slots:
    void pause();
    void resume();
    void seek(int64_t, int64_t); // us
    void mute(bool);
    void setSpeed(float);
    void setVolume(int);
    void openFile();
    void showPreferences();
    void showProperties();

signals:
    void started();
    void timeChanged(int64_t);

protected:
    void closeEvent(QCloseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    void contextMenuEvent(QContextMenuEvent *) override;

    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void video_thread_f();

    void initContextMenu();

    // UI
    TextureGLWidget *texture_{};

    ControlWidget *control_{};
    QTimer *timer_{};

    Menu *menu_{};
    QMenu *asmenu_{};
    QActionGroup *asgroup_{};
    QMenu *ssmenu_{};
    QActionGroup *ssgroup_{};

    // video & audio
    std::thread video_thread_;

    std::unique_ptr<Decoder> decoder_{ nullptr };
    std::unique_ptr<AudioRender> audio_render_{ nullptr };
    std::unique_ptr<Dispatcher> dispatcher_{ nullptr }; // TODO: ctor & dtor order

    std::atomic<bool> video_enabled_{ false };
    std::atomic<bool> audio_enabled_{ false };

    lock_queue<av::frame> vbuffer_{};
    AVAudioFifo *abuffer_{ nullptr };
    std::atomic<int64_t> audio_pts_{ AV_NOPTS_VALUE };

    std::atomic<bool> seeking_{ false };
    std::atomic<int> step_{ 0 };

    std::atomic<bool> paused_{ false };

    // clock @{
    avsync_t avsync_{ avsync_t::AUDIO_MASTER };

    struct clock_t
    {
        std::atomic<int64_t> pts{ AV_NOPTS_VALUE };

        std::atomic<float> speed{ 1.0 };
        std::atomic<int64_t> updated{ AV_NOPTS_VALUE };
    } clock_{};
    // @}
};

#endif // !CAPTURER_VIDEO_PLAYER_H