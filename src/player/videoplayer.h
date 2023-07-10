#ifndef CAPTURER_VIDEO_PLAYER_H
#define CAPTURER_VIDEO_PLAYER_H

#include "control-widget.h"
#include "framelesswindow.h"
#include "libcap/consumer.h"
#include "libcap/decoder.h"
#include "libcap/dispatcher.h"
#include "texture-widget.h"
#include "texture-widget-opengl.h"

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

    int64_t clock_ns() { return os_gettime_ns() - first_pts_; }

signals:
    void started();
    void timeChanged(int64_t);

protected:
    void closeEvent(QCloseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    void video_thread_f();
    void audio_thread_f();

    int64_t first_pts_{ AV_NOPTS_VALUE };
    std::atomic<int64_t> audio_clock_{ 0 }; // { 1, AV_TIME_BASE } unit
    std::atomic<int64_t> audio_clock_ts_{ 0 };

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
    int64_t paused_pts_{ AV_NOPTS_VALUE };
};

#endif // !CAPTURER_VIDEO_PLAYER_H