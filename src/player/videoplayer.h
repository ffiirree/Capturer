#ifndef CAPTURER_VIDEO_PLAYER_H
#define CAPTURER_VIDEO_PLAYER_H

#include "control-widget.h"
#include "framelesswindow.h"
#include "libcap/consumer.h"
#include "libcap/decoder.h"
#include "libcap/dispatcher.h"
#include "texture-widget-opengl.h"
#include "texture-widget.h"

#include <QTimer>

class VideoPlayer : public FramelessWindow, public Consumer<AVFrame>
{
    Q_OBJECT

public:
    explicit VideoPlayer(QWidget *parent = nullptr);
    ~VideoPlayer() override;

    int open(const std::string&, std::map<std::string, std::string>) override;

    int run() override { return 0; };

    [[nodiscard]] bool ready() const override { return decoder_ && decoder_->ready(); }

    void reset() override {}

    int consume(AVFrame *frame, int type) override;

    [[nodiscard]] bool full(int type) const override
    {
        switch (type) {
        case AVMEDIA_TYPE_VIDEO: return video_buffer_.full();
        case AVMEDIA_TYPE_AUDIO: return audio_buffer_.full();
        default: return true;
        }
    }

    void enable(int type, bool v = true) override
    {
        switch (type) {
        case AVMEDIA_TYPE_VIDEO: video_enabled_ = v; break;
        case AVMEDIA_TYPE_AUDIO: audio_enabled_ = v; break;
        }
    }

    [[nodiscard]] bool accepts(int type) const override
    {
        switch (type) {
        case AVMEDIA_TYPE_VIDEO: return video_enabled_;
        case AVMEDIA_TYPE_AUDIO: return audio_enabled_;
        default: return false;
        }
    }

    int64_t clock_ns()
    {
        if (offset_ts_ == AV_NOPTS_VALUE) return 0;
        if (speed_.ts == AV_NOPTS_VALUE) return os_gettime_ns() - offset_ts_;

        int64_t normal = speed_.ts - offset_ts_;
        int64_t speedx = (os_gettime_ns() - speed_.ts) * speed_.x;
        return normal + speedx;
    }

    bool paused() const { return paused_pts_ != AV_NOPTS_VALUE; }

public slots:
    void pause();
    void resume();
    void seek(int64_t, int64_t); // us
    void mute(bool);
    void setSpeed(float);
    void setVolume(int);

signals:
    void started();
    void timeChanged(int64_t);

protected:
    void closeEvent(QCloseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void video_thread_f();
    void audio_thread_f();

    std::atomic<bool> video_enabled_{ false };
    std::atomic<bool> audio_enabled_{ false };

    TextureGLWidget *texture_{};
    Decoder *decoder_{ nullptr };
    Dispatcher *dispatcher_{ nullptr };

    std::thread video_thread_;
    std::thread audio_thread_;

    RingVector<AVFrame *, 8> video_buffer_{
        []() { return av_frame_alloc(); },
        [](AVFrame **frame) { av_frame_free(frame); },
    };

    RingVector<AVFrame *, 32> audio_buffer_{
        []() { return av_frame_alloc(); },
        [](AVFrame **frame) { av_frame_free(frame); },
    };

    ControlWidget *control_{};
    QTimer *timer_{};

    // pause / resume
    std::atomic<bool> seeking_{ false };
    std::atomic<int64_t> paused_pts_{ AV_NOPTS_VALUE };
    std::atomic<int> step_{ 0 };

    std::atomic<int64_t> offset_ts_{ AV_NOPTS_VALUE }; // OS_TIME_BASE
    std::atomic<int64_t> video_pts_{ AV_NOPTS_VALUE }; // OS_TIME_BASE

    struct speed_t
    {
        std::atomic<float> x{ 1.0 };
        std::atomic<int64_t> ts{ AV_NOPTS_VALUE };
    } speed_{};

    struct volume_t
    {
        std::atomic<int> x{ 1 };
        std::atomic<bool> muted{ false };
    } volume_;
};

#endif // !CAPTURER_VIDEO_PLAYER_H