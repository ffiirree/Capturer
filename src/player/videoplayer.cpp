#include "videoplayer.h"

#include "logging.h"
#include "probe/thread.h"

#include <fmt/chrono.h>
#include <QCheckBox>
#include <QChildEvent>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QShortcut>
#include <QStackedLayout>
#include <QVBoxLayout>

VideoPlayer::VideoPlayer(QWidget *parent)
    : FramelessWindow(parent)
{
    installEventFilter(this);

    decoder_    = new Decoder();
    dispatcher_ = new Dispatcher();

    texture_ = new TextureGLWidget();

    auto stacked_layout = new QStackedLayout(this);
    stacked_layout->setStackingMode(QStackedLayout::StackAll);
    setLayout(stacked_layout);

    control_ = new ControlWidget(this);
    connect(control_, &ControlWidget::pause, this, &VideoPlayer::pause);
    connect(control_, &ControlWidget::resume, this, &VideoPlayer::resume);
    connect(control_, &ControlWidget::seek, this, &VideoPlayer::seek);
    connect(control_, &ControlWidget::speed, this, &VideoPlayer::speed);
    connect(this, &VideoPlayer::timeChanged, [this](auto t) {
        if (!seeking_) control_->setTime(t);
    });
    stacked_layout->addWidget(control_);
    stacked_layout->addWidget(texture_);

    timer_ = new QTimer;
    connect(timer_, &QTimer::timeout, [this]() {
        setCursor(Qt::BlankCursor);

        // control_->hide();
        timer_->stop();
    });
    timer_->start(2000ms);

    setMouseTracking(true);

    // clang-format off
    connect(new QShortcut(Qt::Key_Right, this), &QShortcut::activated, [this]() { seek(clock_ns() / 1'000 + 10'000'000, clock_ns() / 1000); }); // +10s
    connect(new QShortcut(Qt::Key_Left,  this), &QShortcut::activated, [this]() { seek(clock_ns() / 1'000 - 10'000'000, clock_ns() / 1000); }); // -10s
    connect(new QShortcut(Qt::Key_Space, this), &QShortcut::activated, [this]() { paused() ? resume() : pause(); });
    // clang-format on
}

VideoPlayer::~VideoPlayer()
{
    delete dispatcher_; // 1.
    delete decoder_;    // 2.
}

int VideoPlayer::open(const std::string& filename, std::map<std::string, std::string> options)
{
    std::map<std::string, std::string> decoder_options;
    if (options.contains("format")) decoder_options["format"] = options.at("format");

    if (decoder_->open(filename, decoder_options) != 0) {
        decoder_->reset();
        return false;
    }

    if (texture_->setPixelFormat(texture_->isSupported(decoder_->vfmt.pix_fmt)
                                     ? decoder_->vfmt.pix_fmt
                                     : texture_->formats()[0]) < 0) {
        return false;
    }

    dispatcher_->append(decoder_);
    dispatcher_->set_encoder(this);

    dispatcher_->afmt.sample_fmt = decoder_->afmt.sample_fmt;
    dispatcher_->vfmt.pix_fmt    = texture_->format();

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

        if (offset_ts_ == AV_NOPTS_VALUE || seeking_) {
            video_pts_ = av_rescale_q(frame->pts, vfmt.time_base, OS_TIME_BASE_Q);
            offset_ts_ = os_gettime_ns() - video_pts_;
            speed_.ts  = os_gettime_ns();

            if (paused()) paused_pts_ = os_gettime_ns();

            LOG(INFO) << fmt::format("seek -> clock: {:%H:%M:%S}", std::chrono::nanoseconds(clock_ns()));
            seeking_ = false;
        }

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

void VideoPlayer::pause()
{
    if (paused_pts_ == AV_NOPTS_VALUE) {
        paused_pts_ = os_gettime_ns();
    }
}

void VideoPlayer::resume()
{
    if (paused_pts_ != AV_NOPTS_VALUE) {
        offset_ts_ += os_gettime_ns() - paused_pts_;
        paused_pts_ = AV_NOPTS_VALUE;
    }
}

void VideoPlayer::seek(int64_t nts, int64_t ots)
{
    if (seeking_) return;

    nts = std::clamp<int64_t>(nts, 0, decoder_->duration());

    dispatcher_->seek(std::chrono::microseconds{ nts });
    seeking_ = true;
    offset_ts_ += (ots - nts) * 1'000;
    video_buffer_.clear();

    LOG(INFO) << fmt::format("seek: {:%H:%M:%S}, clock: {:%H:%M:%S}", std::chrono::microseconds(nts),
                             std::chrono::nanoseconds(clock_ns()));

    if (paused()) {
        step_ = 1;
    }
}

void VideoPlayer::speed(float speed)
{
    offset_ts_ = os_gettime_ns() - video_pts_;
    speed_.ts  = os_gettime_ns();
    speed_.x   = std::clamp<float>(speed, 0.5, 3.0);
    ;
}

void VideoPlayer::video_thread_f()
{
    auto frame = av_frame_alloc();
    defer(av_frame_free(&frame));

    probe::thread::set_name("player-video");

    while (video_enabled_ && running_) {
        if (video_buffer_.empty() || (paused() && !step_)) {
            std::this_thread::sleep_for(20ms);
            continue;
        }

        video_buffer_.pop([=, this](AVFrame *popped) {
            av_frame_unref(frame);
            av_frame_move_ref(frame, popped);
        });

        if (!step_ && !seeking_) {
            video_pts_       = av_rescale_q(frame->pts, vfmt.time_base, OS_TIME_BASE_Q);
            int64_t sleep_ns = std::clamp<int64_t>((video_pts_ - clock_ns()) / speed_.x, 0, OS_TIME_BASE);

            DLOG(INFO) << fmt::format("[V] pts = {}ms, time = {}ms, sleep = {}ms, speed = {:4.2f}x",
                                      av_rescale_q(frame->pts, vfmt.time_base, { 1, 1'000 }),
                                      clock_ns() / 1'000'000, sleep_ns / 1'000'000, speed_.x.load());
            os_nsleep(sleep_ns);
        }

        texture_->present(frame);
        emit timeChanged(av_rescale_q(frame->pts, vfmt.time_base, { 1, AV_TIME_BASE }));

        step_ = std::max<int>(0, step_ - 1);
    }
}

void VideoPlayer::audio_thread_f()
{
    probe::thread::set_name("player-audio");

    while (audio_enabled_ && running_) {
        std::this_thread::sleep_for(100ms);
    }
}

bool VideoPlayer::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::ChildAdded) {
        dynamic_cast<QChildEvent *>(event)->child()->installEventFilter(this);
        qDebug() << Q_FUNC_INFO << object << event;
    }

    if (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress) {
        setCursor(Qt::ArrowCursor);
        dynamic_cast<QStackedLayout *>(layout())->widget(0)->show();
        timer_->start(2'000ms);
    }

    return false;
}

void VideoPlayer::closeEvent(QCloseEvent *event)
{
    eof_ = 0x01;

    if (dispatcher_) {
        dispatcher_->stop();
    }

    running_ = false;

    if (video_thread_.joinable()) video_thread_.join();
    if (audio_thread_.joinable()) audio_thread_.join();

    offset_ts_ = AV_NOPTS_VALUE;

    FramelessWindow::closeEvent(event);
}

void VideoPlayer::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        control_->paused() ? control_->resume() : control_->pause();
    }
    FramelessWindow::mouseDoubleClickEvent(event);
}