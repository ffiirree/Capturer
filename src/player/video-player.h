#ifndef CAPTURER_PLAYER_H
#define CAPTURER_PLAYER_H

#include "control-widget.h"
#include "decoder.h"
#include "framelesswindow.h"
#include "libcap/audio-renderer.h"
#include "libcap/sonic.h"
#include "libcap/timeline.h"
#include "menu.h"
#include "texture-widget-d3d11.h"
#include "texture-widget-opengl.h"

#include <QTimer>

class VideoPlayer final : public FramelessWindow
{
    Q_OBJECT

public:
    explicit VideoPlayer(QWidget *parent = nullptr);

    ~VideoPlayer() override;

    // open the file & playback
    int open(const std::string& filename);

    int start();

    bool paused() const { return paused_; }

    void stop();

    int consume(const av::frame& frame, AVMediaType type);

public slots:
    void pause();
    void resume();

    void seek(std::chrono::nanoseconds, std::chrono::nanoseconds); // us

    void mute(bool);

    void setSpeed(float);

    void setVolume(int);

    void showProperties();

    void finish();

signals:
    void started();
    void timeChanged(int64_t);

    void videoFinished();
    void audioFinished();

protected:
    bool event(QEvent *event) override;

    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    void contextMenuEvent(QContextMenuEvent *) override;

private:
    void initContextMenu();

    void     video_thread_fn();
    uint32_t audio_callback(uint8_t **ptr, uint32_t request_frames, std::chrono::nanoseconds ts);

    std::string filename_{};

    // UI
    ControlWidget   *control_{};
    TextureGLWidget *texture_{};

    QTimer *timer_{};

    Menu         *menu_{};
    QMenu        *asmenu_{};
    QActionGroup *asgroup_{};
    QMenu        *ssmenu_{};
    QActionGroup *ssgroup_{};

    // state
    std::atomic<bool> ready_{};
    std::atomic<bool> running_{};
    std::atomic<bool> paused_{};

    bool is_live_{};

    // video & audio
    std::jthread video_thread_{};

    std::unique_ptr<Decoder>       source_{};
    std::unique_ptr<AudioRenderer> audio_renderer_{};

    std::atomic<bool> video_enabled_{};
    std::atomic<bool> audio_enabled_{};

    sonic_stream *sonic_stream_{}; // audio speed up / down

    std::atomic<bool>     seeking_{};
    safe_queue<av::frame> aqueue_{ 2 };
    std::atomic<bool>     adone_{};
    safe_queue<av::frame> vqueue_{ 2 };
    std::atomic<bool>     vdone_{};

    std::atomic<int> vstep_{ 0 };
    std::atomic<int> astep_{ 0 };

    std::atomic<std::chrono::nanoseconds> audio_pts_{ av::clock::nopts };
    av::timeline_t                        timeline_{ av::clock::nopts };
};

#endif //! CAPTURER_PLAYER_H