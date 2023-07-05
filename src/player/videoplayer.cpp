#include "videoplayer.h"

#include "logging.h"
#include "probe/thread.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QStackedLayout>
#include <QVBoxLayout>

VideoPlayer::VideoPlayer(QWidget *parent)
    : FramelessWindow(parent)
{
    setWindowFlags(windowFlags() | Qt::Window | Qt::WindowStaysOnTopHint);

    decoder_    = new Decoder();
    dispatcher_ = new Dispatcher();

    texture_ = new TextureWidget();

    auto stacked_layout = new QStackedLayout(this);
    stacked_layout->setStackingMode(QStackedLayout::StackAll);
    setLayout(stacked_layout);

    control_ = new ControlWidget(this);
    connect(control_, &ControlWidget::pause, [this]() { paused_pts_ = os_gettime_ns(); });
    connect(control_, &ControlWidget::resume, [this]() {
        if (paused_pts_ != AV_NOPTS_VALUE) {
            first_pts_ += os_gettime_ns() - paused_pts_;
            paused_pts_ = AV_NOPTS_VALUE;
        }
    });
    connect(this, &VideoPlayer::timeChanged, [this](auto t) { control_->setTime(t); });
    stacked_layout->addWidget(control_);
    stacked_layout->addWidget(texture_);

    timer_ = new QTimer;
    connect(timer_, &QTimer::timeout, [this]() {
        control_->hide();
        timer_->stop();
    });
    timer_->start();

    setMouseTracking(true);
}

VideoPlayer::~VideoPlayer()
{
    delete dispatcher_; // 1.
    delete decoder_;    // 2.

    running_ = false;

    if (video_thread_.joinable()) video_thread_.join();
    if (audio_thread_.joinable()) audio_thread_.join();

    first_pts_ = AV_NOPTS_VALUE;
}

int VideoPlayer::open(const std::string& filename, std::map<std::string, std::string> options)
{
    std::map<std::string, std::string> decoder_options;
    if (options.contains("format")) decoder_options["format"] = options.at("format");

    if (decoder_->open(filename, decoder_options) != 0) {
        decoder_->reset();
        return false;
    }

    dispatcher_->append(decoder_);
    dispatcher_->set_encoder(this);

    dispatcher_->vfmt.pix_fmt = AV_PIX_FMT_RGBA;
    if (dispatcher_->create_filter_graph(options.contains("filters") ? options.at("filters") : "", {})) {
        LOG(INFO) << "create filters failed";
        return false;
    }

    if (vfmt.width > 1'920 && vfmt.height > 1'080) {
        resize(QSize(vfmt.width, vfmt.height).scaled(1'920, 1'080, Qt::KeepAspectRatio));
    }
    else {
        resize(vfmt.width, vfmt.height);
    }

    eof_ = 0x00;

    if (dispatcher_->start() < 0) {
        dispatcher_->reset();
        LOG(INFO) << "failed to start";
        return false;
    }

    QWidget::setWindowTitle(QString::fromUtf8(filename.c_str()));
    control_->setDuration(decoder_->duration());

    running_ = true;

    // threads
    video_thread_ = std::thread([this]() { this->video_thread_f(); });
    audio_thread_ = std::thread([this]() { this->audio_thread_f(); });

    QWidget::show();

    emit started();
    return true;
}

int VideoPlayer::consume(AVFrame *frame, int type)
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO:
        if (video_buffer_.full()) return AVERROR(EAGAIN);

        video_buffer_.push([frame](AVFrame *p_frame) {
            av_frame_unref(p_frame);
            av_frame_move_ref(p_frame, frame);
        });
        return 0;

    case AVMEDIA_TYPE_AUDIO:
        // if (audio_buffer_.full()) return -1;

        // audio_buffer_.push([frame](AVFrame *p_frame) {
        //     av_frame_unref(p_frame);
        //     av_frame_move_ref(p_frame, frame);
        // });
        return 0;

    default: return -1;
    }
}

void VideoPlayer::video_thread_f()
{
    auto frame = av_frame_alloc();
    defer(av_frame_free(&frame));

    probe::thread::set_name("player-video");

    while (video_enabled_ && running_) {
        if (video_buffer_.empty() || paused_pts_ != AV_NOPTS_VALUE) {
            std::this_thread::sleep_for(20ms);
            continue;
        }

        video_buffer_.pop([=, this](AVFrame *popped) {
            av_frame_unref(frame);
            av_frame_move_ref(frame, popped);
        });

        first_pts_ = (first_pts_ == AV_NOPTS_VALUE) ? os_gettime_ns() : first_pts_;

        int64_t pts_ns = av_rescale_q(frame->pts, vfmt.time_base, OS_TIME_BASE_Q);
        int64_t sleep_ns =
            std::min<int64_t>(std::max<int64_t>(0, pts_ns - clock_ns() - 3 * 1'000'000), OS_TIME_BASE);

        LOG(INFO) << fmt::format("pts = {}, time = {}ms, sleep = {}ms", frame->pts, clock_ns() / 1'000'000,
                                 sleep_ns / 1'000'000);
        os_nsleep(sleep_ns);

        texture_->present(frame);
        emit timeChanged(av_rescale_q(frame->pts, vfmt.time_base, { 1, 1 }));
    }
}

void VideoPlayer::audio_thread_f()
{
    probe::thread::set_name("player-audio");

    while (audio_enabled_ && running_) {
        std::this_thread::sleep_for(100ms);
    }
}

void VideoPlayer::closeEvent(QCloseEvent *event)
{
    eof_ = 0x01;

    if (dispatcher_) {
        dispatcher_->stop();
    }

    FramelessWindow::closeEvent(event);
}

void VideoPlayer::mouseMoveEvent(QMouseEvent *event)
{
    dynamic_cast<QStackedLayout *>(layout())->widget(0)->show();
    timer_->stop();
    timer_->setInterval(2'000ms);
    timer_->start();
    FramelessWindow::mouseMoveEvent(event);
}

void VideoPlayer::mouseDoubleClickEvent(QMouseEvent *event)
{
    control_->paused() ? control_->resume() : control_->pause();
    FramelessWindow::mouseDoubleClickEvent(event);
}