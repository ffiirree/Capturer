#include "videoplayer.h"

#include "logging.h"

#include <fmt/chrono.h>
#include <libcap/devices.h>
#include <probe/thread.h>
#include <QCheckBox>
#include <QChildEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QShortcut>
#include <QStackedLayout>
#include <QVBoxLayout>

#ifdef _WIN32
#include <libcap/win-wasapi/wasapi-render.h>
#include <libcap/win-wasapi/win-wasapi.h>
#elif __linux__
#include <libcap/linux-pulse/pulse-render.h>
#endif

using namespace std::chrono;

VideoPlayer::VideoPlayer(QWidget *parent)
    : FramelessWindow(parent, Qt::WindowMinMaxButtonsHint | Qt::WindowFullscreenButtonHint |
                                  Qt::WindowStaysOnTopHint)
{
    setMouseTracking(true);
    installEventFilter(this);

    decoder_    = std::make_unique<Decoder>();
    dispatcher_ = std::make_unique<Dispatcher>();
#ifdef _WIN32
    audio_render_ = std::make_unique<WasapiRender>();
#else
    audio_render_ = std::make_unique<PulseAudioRender>();
#endif

    auto stacked_layout = new QStackedLayout(this);
    stacked_layout->setStackingMode(QStackedLayout::StackAll);
    setLayout(stacked_layout);

    // control bar
    control_ = new ControlWidget(this);
    connect(control_, &ControlWidget::pause, this, &VideoPlayer::pause);
    connect(control_, &ControlWidget::resume, this, &VideoPlayer::resume);
    connect(control_, &ControlWidget::seek, this, &VideoPlayer::seek);
    connect(control_, &ControlWidget::speed, this, &VideoPlayer::setSpeed);
    connect(control_, &ControlWidget::volume, [this](int val) { audio_render_->setVolume(val / 100.0f); });
    connect(control_, &ControlWidget::mute, [this](bool muted) { audio_render_->mute(muted); });
    connect(this, &VideoPlayer::timeChanged, [this](auto t) {
        if (!seeking_) control_->setTime(t);
    });
    stacked_layout->addWidget(control_);

    // texture
    texture_ = new TextureGLWidget();
    stacked_layout->addWidget(texture_);

    //
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, [this]() {
        setCursor(Qt::BlankCursor);
        control_->hide();
        timer_->stop();
    });
    timer_->start(2'000ms);

    // clang-format off
    connect(new QShortcut(Qt::Key_Right, this), &QShortcut::activated, [this]() { seek(clock_ns() / 1'000 + 10'000'000, + 10'000'000); }); // +10s
    connect(new QShortcut(Qt::Key_Left,  this), &QShortcut::activated, [this]() { seek(clock_ns() / 1'000 - 10'000'000, - 10'000'000); }); // -10s
    connect(new QShortcut(Qt::Key_Space, this), &QShortcut::activated, [this]() { control_->paused() ? control_->resume() : control_->pause(); });
    connect(new QShortcut(Qt::Key_Up,    this), &QShortcut::activated, [this]() { control_->setVolume(audio_render_->volume() * 100 + 5); }); // +10s
    connect(new QShortcut(Qt::Key_Down,  this), &QShortcut::activated, [this]() { control_->setVolume(audio_render_->volume() * 100 - 5); }); // -10s
    // clang-format on
}

void VideoPlayer::reset()
{
    running_ = false;
    if (video_thread_.joinable()) video_thread_.join();
    audio_render_->stop();

    eof_   = 0x00;
    ready_ = false;

    video_enabled_ = false;
    audio_enabled_ = false;

    vbuffer_.clear();
    if (abuffer_) av_audio_fifo_reset(abuffer_);
    audio_pts_ = AV_NOPTS_VALUE;
    step_      = 0;
    paused_    = false;

    set_clock(AV_NOPTS_VALUE);
    set_speed(1.0);
}

VideoPlayer ::~VideoPlayer()
{
    running_ = false;
    if (video_thread_.joinable()) video_thread_.join();
}

int VideoPlayer::open(const std::string& filename, std::map<std::string, std::string> options)
{
    std::map<std::string, std::string> decoder_options;
    if (options.contains("format")) decoder_options["format"] = options.at("format");

    // video decoder
    decoder_->set_timing(av::timing_t::none);
    if (decoder_->open(filename, decoder_options) != 0) {
        decoder_->reset();
        LOG(INFO) << "[    PLAYER] failed to open video decoder";
        return -1;
    }

    dispatcher_->append(decoder_.get());

    // audio render
    auto default_render = av::default_audio_sink();
    if (!default_render || audio_render_->open(default_render->id, {}) != 0) {
        audio_render_->reset();
        LOG(INFO) << "[    PLAYER] failed to open audio render";
        return -1;
    }

    afmt = audio_render_->format();
    setVolume(static_cast<int>(audio_render_->volume() * 100));
    mute(audio_render_->muted());

    if (abuffer_) av_audio_fifo_free(abuffer_);
    if (abuffer_ = av_audio_fifo_alloc(afmt.sample_fmt, afmt.channels, 1); !abuffer_) {
        LOG(ERROR) << "[    PLAYER] failed to alloc audio buffer";
        return -1;
    }

    dispatcher_->afmt = audio_render_->format();

    // dispatcher
    dispatcher_->set_encoder(this);

    dispatcher_->vfmt.pix_fmt =
        texture_->isSupported(decoder_->vfmt.pix_fmt) ? decoder_->vfmt.pix_fmt : texture_->pix_fmts()[0];

    dispatcher_->set_timing(av::timing_t::device);
    if (dispatcher_->initialize((options.contains("filters") ? options.at("filters") : ""), "atempo=1.0")) {
        LOG(INFO) << "[    PLAYER] failed to create filter graph";
        return false;
    }

    if (texture_->setFormat(vfmt) < 0) {
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
        LOG(INFO) << "[    PLAYER] failed to start dispatcher";
        return false;
    }

    QFileInfo info(QString::fromStdString(filename));
    QWidget::setWindowTitle(info.isFile() ? info.fileName() : info.filePath());
    control_->setDuration(decoder_->duration());

    return true;
}

bool VideoPlayer::full(AVMediaType type) const
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO: return vbuffer_.size() > 2;
    case AVMEDIA_TYPE_AUDIO: return av_audio_fifo_size(abuffer_) > 1'024;
    default: return true;
    }
}

void VideoPlayer::enable(AVMediaType type, bool v)
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO: video_enabled_ = v; break;
    case AVMEDIA_TYPE_AUDIO: audio_enabled_ = v; break;
    default: break;
    }
}

bool VideoPlayer::accepts(AVMediaType type) const
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO: return video_enabled_;
    case AVMEDIA_TYPE_AUDIO: return audio_enabled_;
    default: return false;
    }
}

int VideoPlayer::consume(AVFrame *frame, AVMediaType type)
{
    if (seeking_ &&
        ((type == AVMEDIA_TYPE_AUDIO && sync_type() == AUDIO_MASTER) || type == AVMEDIA_TYPE_VIDEO)) {
        auto time_base = (type == AVMEDIA_TYPE_VIDEO) ? vfmt.time_base : afmt.time_base;
        set_clock(av_rescale_q(frame->pts, time_base, OS_TIME_BASE_Q));
        LOG(INFO) << fmt::format("[{}] clock: {:%H:%M:%S}", av::to_char(type), nanoseconds(clock_ns()));
        seeking_ = false;
    }

    switch (type) {
    case AVMEDIA_TYPE_VIDEO:
        if (!frame || !frame->data[0]) {
            eof_ |= 0x01;
            LOG(INFO) << "[    PLAYER] [V] EOF";
            return AVERROR_EOF;
        }

        if (full(AVMEDIA_TYPE_VIDEO)) return AVERROR(EAGAIN);

        vbuffer_.emplace(frame);
        return 0;

    case AVMEDIA_TYPE_AUDIO: {
        if (!frame || !frame->data[0]) {
            eof_ |= 0x02;
            LOG(INFO) << "[    PLAYER] [A] EOF";
            return AVERROR_EOF;
        }

        if (full(AVMEDIA_TYPE_AUDIO)) return AVERROR(EAGAIN);

        std::lock_guard lock(mtx_);

        auto buffer_size = av_audio_fifo_size(abuffer_);
        if (av_audio_fifo_realloc(abuffer_, buffer_size + frame->nb_samples) < 0) return -1;
        av_audio_fifo_write(abuffer_, reinterpret_cast<void **>(frame->data), frame->nb_samples);

        // TODO: speed
        if (audio_pts_ == AV_NOPTS_VALUE) audio_pts_ = frame->pts;
        audio_pts_ +=
            av_rescale_q(frame->nb_samples, { 1, afmt.sample_rate }, afmt.time_base) * get_speed();
        return 0;
    }

    default: return -1;
    }
}

void VideoPlayer::pause()
{
    set_clock(clock_ns());
    audio_render_->pause();
    paused_ = true;
}

void VideoPlayer::resume()
{
    if (eof()) {
        seek(0, 0);
        eof_ = 0;
    }

    set_clock(clock_ns());
    paused_ = false;
    audio_render_->resume();
}

void VideoPlayer::seek(int64_t ts, int64_t rel)
{
    if (seeking_) return;

    std::lock_guard lock(mtx_);

    ts = std::clamp<int64_t>(ts, 0, decoder_->duration());

    LOG(INFO) << fmt::format("seek: {:%H:%M:%S}", std::chrono::microseconds(ts));

    auto lts = rel < 0 ? std::numeric_limits<int64_t>::min() : ts - rel * 3 / 4;
    auto rts = rel > 0 ? std::numeric_limits<int64_t>::max() : ts - rel * 3 / 4;
    dispatcher_->seek(std::chrono::microseconds{ ts }, lts, rts);

    seeking_ = true;
    eof_     = 0;
    set_clock(AV_NOPTS_VALUE);
    audio_pts_ = AV_NOPTS_VALUE;

    vbuffer_.clear();
    av_audio_fifo_reset(abuffer_);
    audio_render_->reset();

    if (paused()) {
        step_ = 1;
    }
}

void VideoPlayer::setSpeed(float speed)
{
    dispatcher_->afilters = fmt::format("atempo={:4.2f}", speed);
    dispatcher_->send_command(AVMEDIA_TYPE_AUDIO, "atempo", "tempo", fmt::format("{:4.2f}", speed));
    set_speed(speed);
}

void VideoPlayer::mute(bool muted) { control_->setMute(muted); }

void VideoPlayer::setVolume(int volume)
{
    volume = std::clamp<int>(volume, 0, 100);

    control_->setVolume(volume);
}

int VideoPlayer::run()
{
    if (running_) {
        LOG(ERROR) << "[    PLAYER] already running";
        return -1;
    }

    running_ = true;
    set_clock(0);

    // video thread
    video_thread_ = std::thread([this]() { video_thread_f(); });

    // audio thread
    if (audio_enabled_ &&
        audio_render_->start([this](uint8_t **ptr, uint32_t request_frames, int64_t ts) -> uint32_t {
            std::lock_guard lock(mtx_);

            uint32_t buffer_frames = av_audio_fifo_size(abuffer_); // per channel
            if (!buffer_frames || seeking_ || paused()) return 0;

            if (sync_type() == AUDIO_MASTER) {
                uint32_t remaining = buffer_frames + (audio_render_->bufferSize() - request_frames);
                set_clock(av_rescale_q(audio_pts_, afmt.time_base, OS_TIME_BASE_Q) -
                              av_rescale_q(remaining, { 1, afmt.sample_rate }, OS_TIME_BASE_Q) *
                                  get_speed(),
                          ts);
            }

            return av_audio_fifo_read(abuffer_, reinterpret_cast<void **>(ptr), request_frames);
        }) < 0) {
        LOG(ERROR) << "failed to start audio render";
        return -1;
    }

    emit started();
    QWidget::show();

    return 0;
}

void VideoPlayer::video_thread_f()
{
    probe::thread::set_name("player-video");

#ifdef _WIN32
    ::timeBeginPeriod(1);
    defer(::timeEndPeriod(1));
#endif

    while (video_enabled_ && running_) {
        if (vbuffer_.empty() || (paused() && !step_) || seeking_) {
            std::this_thread::sleep_for(10ms);
            continue;
        }

        // TODO: vbuffer is cleared, then pop on empty quene case a crush
        av::frame frame(vbuffer_.pop());

        if (!step_) {
            auto pts      = av_rescale_q(frame->pts, vfmt.time_base, OS_TIME_BASE_Q);
            auto sleep_ns = std::clamp<int64_t>((pts - clock_ns()) / get_speed(), 1'000'000, 250'000'000);

            DLOG(INFO) << fmt::format(
                "[V] pts = {:%H:%M:%S}, time = {:%H:%M:%S}, sleep = {:7.3f}ms, speed = {:4.2f}x",
                milliseconds{ pts / 1'000'000 }, milliseconds{ clock_ns() / 1'000'000 },
                sleep_ns / 1'000'000.0, get_speed());

            os_nsleep(sleep_ns);
        }

        texture_->present(frame);
        emit timeChanged(av_rescale_q(frame->pts, vfmt.time_base, { 1, AV_TIME_BASE }));

        step_ = std::max<int>(0, step_ - 1);
    }
}

bool VideoPlayer::eventFilter(QObject *, QEvent *event)
{
    if (event->type() == QEvent::ChildAdded)
        dynamic_cast<QChildEvent *>(event)->child()->installEventFilter(this);

    if (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress) {
        setCursor(Qt::ArrowCursor);
        dynamic_cast<QStackedLayout *>(layout())->widget(0)->show();
        timer_->start(2'000ms);
    }

    return false;
}

void VideoPlayer::closeEvent(QCloseEvent *event)
{
    stop();

    FramelessWindow::closeEvent(event);
}

void VideoPlayer::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        control_->paused() ? control_->resume() : control_->pause();
    }
    FramelessWindow::mouseDoubleClickEvent(event);
}