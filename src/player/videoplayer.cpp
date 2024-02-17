#include "videoplayer.h"

#include "libcap/devices.h"
#include "libcap/sonic.h"
#include "logging.h"
#include "titlebar.h"

#include <fmt/chrono.h>
#include <fmt/ranges.h>
#include <probe/graphics.h>
#include <probe/thread.h>
#include <QActionGroup>
#include <QFileInfo>
#include <QHeaderView>
#include <QMouseEvent>
#include <QShortcut>
#include <QStackedLayout>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>

#ifdef _WIN32
#include "libcap/win-wasapi/wasapi-renderer.h"
#elif __linux__
#include "libcap/linux-pulse/pulse-renderer.h"
#endif

VideoPlayer::VideoPlayer(QWidget *parent)
    : FramelessWindow(parent, Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint |
                                  Qt::WindowFullscreenButtonHint | Qt::WindowStaysOnTopHint)
{
    setMouseTracking(true);
    installEventFilter(this);

    decoder_    = std::make_unique<Decoder>();
    dispatcher_ = std::make_unique<Dispatcher>();
#ifdef _WIN32
    audio_renderer_ = std::make_unique<WasapiRenderer>();
#else
    audio_renderer_ = std::make_unique<PulseAudioRenderer>();
#endif

    const auto stacked_layout = new QStackedLayout(this);
    stacked_layout->setStackingMode(QStackedLayout::StackAll);
    setLayout(stacked_layout);

    // control bar
    control_ = new ControlWidget(this);
    connect(control_, &ControlWidget::pause, this, &VideoPlayer::pause);
    connect(control_, &ControlWidget::resume, this, &VideoPlayer::resume);
    connect(control_, &ControlWidget::seek, this, &VideoPlayer::seek);
    connect(control_, &ControlWidget::speed, this, &VideoPlayer::setSpeed);
    connect(control_, &ControlWidget::volume,
            [this](auto val) { audio_renderer_->setVolume(val / 100.0f); });
    connect(control_, &ControlWidget::mute, [this](auto muted) { audio_renderer_->mute(muted); });
    connect(this, &VideoPlayer::timeChanged, control_, &ControlWidget::setTime, Qt::QueuedConnection);
    connect(this, &VideoPlayer::video_finished, this, &VideoPlayer::finish, Qt::QueuedConnection);
    connect(this, &VideoPlayer::audio_finished, this, &VideoPlayer::finish, Qt::QueuedConnection);

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
    timer_->start(2000ms);

    // clang-format off
    connect(new QShortcut(Qt::Key_Right, this), &QShortcut::activated, [this] { seek(timeline_.time() + 10s, + 10s); });
    connect(new QShortcut(Qt::Key_Left,  this), &QShortcut::activated, [this] { seek(timeline_.time() - 10s, - 10s); });
    connect(new QShortcut(Qt::Key_Space, this), &QShortcut::activated, [this] { control_->paused() ? control_->resume() : control_->pause(); });
    connect(new QShortcut(Qt::Key_Up,    this), &QShortcut::activated, [this] { control_->setVolume(audio_renderer_->volume() * 100 + 5); });
    connect(new QShortcut(Qt::Key_Down,  this), &QShortcut::activated, [this] { control_->setVolume(audio_renderer_->volume() * 100 - 5); });
    // clang-format on

    initContextMenu();
}

void VideoPlayer::reset()
{
    running_ = false;
    if (video_thread_.joinable()) video_thread_.join();
    audio_renderer_->stop();

    eof_   = AV_EOF;
    ready_ = false;

    video_enabled_ = false;
    audio_enabled_ = false;

    vbuffer_.clear();
    abuffer_.clear();

    if (sonic_stream_ != nullptr) {
        sonic_stream_destroy(sonic_stream_);
        sonic_stream_ = nullptr;
    };

    audio_pts_ = av::clock::nopts;
    step_      = 0;
    paused_    = false;

    timeline_ = av::clock::nopts;
    timeline_.set_speed({ 1, 1 });
}

VideoPlayer::~VideoPlayer()
{
    running_ = false;
    if (video_thread_.joinable()) video_thread_.join();
}

int VideoPlayer::open(const std::string& filename, std::map<std::string, std::string> options)
{
    filename_ = filename;

    std::map<std::string, std::string> decoder_options;
    if (options.contains("format")) decoder_options["format"] = options.at("format");

#if 0
    decoder_options["hwaccel"]         = "d3d11va";
    decoder_options["hwaccel_pix_fmt"] = "d3d11";
#endif

    // video decoder
    decoder_->set_clock(av::clock_t::none);
    if (decoder_->open(filename, decoder_options) != 0) {
        decoder_->reset();
        LOG(INFO) << "[    PLAYER] failed to open video decoder";
        return -1;
    }

    dispatcher_->append(decoder_.get());

    // audio renderer
    if (const auto default_asink = av::default_audio_sink();
        !default_asink || audio_renderer_->open(default_asink->id, {}) != 0) {
        audio_renderer_->reset();
        LOG(INFO) << "[    PLAYER] failed to open audio render";
        return -1;
    }

    afmt = audio_renderer_->format();
    setVolume(static_cast<int>(audio_renderer_->volume() * 100));
    mute(audio_renderer_->muted());

    if (sonic_stream_ = sonic_stream_create(afmt.sample_fmt, afmt.sample_rate, afmt.channels);
        !sonic_stream_) {
        LOG(ERROR) << "[    PLAYER] failed to create sonic";
        return false;
    }

    dispatcher_->afmt = audio_renderer_->format();

    // dispatcher
    dispatcher_->set_encoder(this);

    dispatcher_->vfmt.pix_fmt =
        texture_->isSupported(decoder_->vfmt.pix_fmt) ? decoder_->vfmt.pix_fmt : texture_->pix_fmts()[0];

    dispatcher_->set_clock(av::clock_t::device);
    if (dispatcher_->initialize((options.contains("filters") ? options.at("filters") : ""), "")) {
        LOG(INFO) << "[    PLAYER] failed to create filter graph";
        return false;
    }

    if (texture_->setFormat(vfmt) < 0) {
        return false;
    }

    if (vfmt.width > 1920 && vfmt.height > 1080) {
        resize(QSize(vfmt.width, vfmt.height).scaled(1920, 1080, Qt::KeepAspectRatio));
    }
    else {
        resize(vfmt.width, vfmt.height);
    }

    eof_ = AV_EOF;

    if (dispatcher_->start() < 0) {
        dispatcher_->reset();
        LOG(INFO) << "[    PLAYER] failed to start dispatcher";
        return false;
    }

    const QFileInfo info(QString::fromStdString(filename));
    setWindowTitle(info.isFile() ? info.fileName() : info.filePath());
    control_->setDuration(decoder_->duration());

    return true;
}

bool VideoPlayer::full(AVMediaType type) const
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO: return vbuffer_.size() >= 2;
    case AVMEDIA_TYPE_AUDIO: return abuffer_.size() >= 16;
    default:                 return true;
    }
}

void VideoPlayer::enable(AVMediaType type, bool v)
{
    // clang-format off
    switch (type) {
    case AVMEDIA_TYPE_VIDEO: video_enabled_  = v; break;
    case AVMEDIA_TYPE_AUDIO: audio_enabled_  = v; break;
    default: break;
    }
    // clang-format on
}

bool VideoPlayer::accepts(AVMediaType type) const
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO: return video_enabled_;
    case AVMEDIA_TYPE_AUDIO: return audio_enabled_;
    default:                 return false;
    }
}

int VideoPlayer::consume(av::frame& frame, AVMediaType type)
{
    if (seeking_ && (type == AVMEDIA_TYPE_AUDIO || type == AVMEDIA_TYPE_VIDEO)) {
        if (frame->pts >= 0) {
            const auto time_base = (type == AVMEDIA_TYPE_VIDEO) ? vfmt.time_base : afmt.time_base;

            timeline_ = av::clock::ns(frame->pts, time_base);

            LOG(INFO) << fmt::format("[{}] clock: {:%T}", av::to_char(type), timeline_.time());
            seeking_ = false;
        }
    }

    switch (type) {
    case AVMEDIA_TYPE_VIDEO:
        if (!frame || !frame->data[0]) {
            eof_     |= VSRC_EOF;
            seeking_  = false;
            if (timeline_.time() == av::clock::nopts && decoder_ && decoder_->duration() != AV_NOPTS_VALUE)
                timeline_ = std::chrono::nanoseconds{ decoder_->duration() * 1000 };
            LOG(INFO) << "[    PLAYER] [V] EOF";
            return AVERROR_EOF;
        }

        if (full(AVMEDIA_TYPE_VIDEO)) return AVERROR(EAGAIN);

        eof_ &= ~(VSRC_EOF | VSINK_EOF);
        vbuffer_.push(frame);
        return 0;

    case AVMEDIA_TYPE_AUDIO: {
        if (!frame || !frame->data[0]) {
            eof_     |= ASRC_EOF;
            seeking_  = false;
            if (timeline_.time() == av::clock::nopts && decoder_ && decoder_->duration() != AV_NOPTS_VALUE)
                timeline_ = std::chrono::nanoseconds{ decoder_->duration() * 1000 };
            LOG(INFO) << "[    PLAYER] [A] EOF";
            return AVERROR_EOF;
        }

        if (full(AVMEDIA_TYPE_AUDIO)) return AVERROR(EAGAIN);

        eof_ &= ~(ASRC_EOF | ASINK_EOF);
        abuffer_.push(frame);
        return 0;
    }

    default: return -1;
    }
}

void VideoPlayer::pause()
{
    timeline_.pause();
    audio_renderer_->pause();
    paused_ = true;
}

void VideoPlayer::resume()
{
    if (eof()) {
        seek(0ns, 0ns);
        eof_ = 0;
    }

    timeline_.resume();
    paused_ = false;
    audio_renderer_->resume();
}

void VideoPlayer::finish()
{
    if (eof()) {
        LOG(INFO) << "[    PLAYER] \"" << filename_ << "\" is finished";
        pause();
        emit control_->pause();
    }
}

void VideoPlayer::seek(std::chrono::nanoseconds ts, std::chrono::nanoseconds rel)
{
    ts = std::clamp<std::chrono::nanoseconds>(ts, 0ns, std::chrono::microseconds{ decoder_->duration() });

    LOG(INFO) << fmt::format("seek: {:%T}", ts);
    emit timeChanged(ts.count() / 1000);

    const auto lts = rel < 0ns ? av::clock::min : ts - rel * 3 / 4;
    const auto rts = rel > 0ns ? av::clock::max : ts - rel * 3 / 4;

    dispatcher_->seek(ts, lts, rts);

    seeking_   = true;
    eof_       = 0;
    timeline_  = av::clock::nopts;
    audio_pts_ = av::clock::nopts;

    eof_ &= ~(ASINK_EOF | VSINK_EOF);

    vbuffer_.clear();
    abuffer_.clear();

    sonic_stream_drain(sonic_stream_);

    audio_renderer_->reset();

    if (paused()) {
        step_ = 1;
    }
}

void VideoPlayer::setSpeed(float speed)
{
    sonic_stream_set_speed(sonic_stream_, speed);
    timeline_.set_speed({ static_cast<intmax_t>(speed * 1000000), 1000000 });
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
    timeline_.set(0ns);

    // video thread
    if (video_enabled_) video_thread_ = std::jthread([this] { video_thread_f(); });

    // audio thread
    audio_renderer_->callback = [this](uint8_t **ptr, auto request_frames, auto ts) -> uint32_t {
        if (seeking_ || (eof_ & ASINK_EOF)) return 0;

        if ((eof_ & ASRC_EOF) && abuffer_.empty()) {
            sonic_stream_flush(sonic_stream_);
            DLOG(INFO) << "flush sonic stream";
            if (sonic_stream_available_samples(sonic_stream_) == 0) {
                eof_ |= ASINK_EOF;
                emit audio_finished();
                DLOG(INFO) << "ASINK_EOF";
            }
        }
        else {
            while (!abuffer_.empty() &&
                   sonic_stream_available_samples(sonic_stream_) < request_frames * 2) {
                const auto frame = abuffer_.pop();
                if (!frame) return 0;

                // update audio clock pts
                if (frame.value()->pts >= 0) audio_pts_ = av::clock::ns(frame.value()->pts, afmt.time_base);

                if (audio_pts_.load() != av::clock::nopts)
                    audio_pts_ =
                        audio_pts_.load() + av::clock::ns(frame.value()->nb_samples, afmt.time_base);

                sonic_stream_write(sonic_stream_, frame.value()->data[0], frame.value()->nb_samples);
            }
        }

        if (audio_pts_.load() != av::clock::nopts) {
            const uint32_t N = sonic_stream_expected_samples(sonic_stream_);    // sonic
            const uint32_t M = audio_renderer_->buffer_size() - request_frames; // renderer
            timeline_.set(audio_pts_.load() - av::clock::ns(N + M, afmt.time_base) * timeline_.speed(), ts);
        }

        return sonic_stream_read(sonic_stream_, *ptr, request_frames);
    };

    if (audio_enabled_ && audio_renderer_->start() < 0) {
        LOG(ERROR) << "[    PLAYER] failed to start audio render";
        return -1;
    }

    emit started();

#ifdef _WIN32
    if (const auto primary_display = probe::graphics::display_contains(0, 0); primary_display.has_value()) {
        move(primary_display->geometry.width / 2 - vfmt.width / 2,
             primary_display->geometry.height / 2 - vfmt.height / 2);
    }
#endif
    QWidget::show();

    return 0;
}

void VideoPlayer::video_thread_f()
{
    probe::thread::set_name("PLAYER-VIDEO");

#ifdef _WIN32
    ::timeBeginPeriod(1);
    defer(::timeEndPeriod(1));
#endif

    while (running_) {
        if (vbuffer_.empty() && (eof_ & VSRC_EOF) && !(eof_ & VSINK_EOF)) {
            eof_ |= VSINK_EOF;
            emit video_finished();
            DLOG(INFO) << "VSINK-EOF";
        }

        if ((eof_ & VSINK_EOF) || vbuffer_.empty() || (paused() && !step_) || seeking_) {
            std::this_thread::sleep_for(5ms);
            continue;
        }

        const auto has_next = vbuffer_.pop();
        if (!has_next) continue;

        av::frame frame = has_next.value();
        texture_->present(frame);

        if (!step_) {
            auto pts      = av::clock::ns(frame->pts, vfmt.time_base);
            auto sleep_ns = std::clamp<std::chrono::nanoseconds>(
                (pts - timeline_.time()) / timeline_.speed(), 1ms, 2500ms);

            DLOG(INFO) << fmt::format("[V] pts = {:%T}, time = {:%T}, sleep = {:.3%S}, speed = {:4.2f}x",
                                      pts, timeline_.time(), sleep_ns, timeline_.speed().get<float>());

            std::this_thread::sleep_for(sleep_ns);
        }

        if (timeline_.time() >= 0ns) emit timeChanged(timeline_.time().count() / 1000);

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
        timer_->start(2000ms);
    }

    return false;
}

void VideoPlayer::closeEvent(QCloseEvent *event)
{
    stop();

    FramelessWindow::closeEvent(event);
}

void VideoPlayer::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::BackButton) {
        seek(timeline_.time() - 10s, -10s);
    }
    else if (event->button() == Qt::ForwardButton) {
        seek(timeline_.time() + 10s, +10s);
    }

    FramelessWindow::mouseReleaseEvent(event);
}

void VideoPlayer::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        control_->paused() ? control_->resume() : control_->pause();
    }
    FramelessWindow::mouseDoubleClickEvent(event);
}

void VideoPlayer::initContextMenu()
{
    menu_ = new Menu(this);

    // video menu
    {
        const auto video_menu = new Menu(tr("Video"), this);

        // video renderers
        {
            const auto renders = new Menu(tr("Renderer"), this);

            const auto group = new QActionGroup(renders);
            group->addAction(renders->addAction("OpenGL"))->setCheckable(true);
            group->addAction(renders->addAction("D3D11"))->setCheckable(true);

            group->actions()[0]->setChecked(true);

            video_menu->addMenu(renders);
        }

        video_menu->addSeparator();

        video_menu->addAction(tr("Rotate +90"));
        video_menu->addAction(tr("Rotate -90"));

        menu_->addMenu(video_menu);
    }

    // audio menu
    {
        const auto audio_menu = new Menu(tr("Audio"), this);

        // audio streams
        {
            asmenu_  = new Menu(tr("Select Audio Stream"), this);
            asgroup_ = new QActionGroup(this);
            audio_menu->addMenu(asmenu_);
        }

        // audio renderers
        {
            const auto renders = new Menu(tr("Renderer"));

            const auto group = new QActionGroup(renders);
            group->addAction(renders->addAction("WASAPI"))->setCheckable(true);
            group->actions()[0]->setChecked(true);

            audio_menu->addMenu(renders);
        }

        menu_->addMenu(audio_menu);
    }

    // subtitle menu
    {
        const auto subtitles_menu = new Menu(tr("Subtitles"), this);

        ssmenu_  = new Menu(tr("Select Subtitles"), this);
        ssgroup_ = new QActionGroup(this);
        subtitles_menu->addMenu(ssmenu_);
        subtitles_menu->addAction(tr("Add Subtitles"));
        subtitles_menu->addAction(tr("Show/Hide Subtitles"))->setCheckable(true);
        menu_->addMenu(subtitles_menu);
    }

    menu_->addSeparator();

    menu_->addAction(tr("Properties"), this, &VideoPlayer::showProperties,
                     QKeySequence(Qt::CTRL | Qt::Key_I));
}

void VideoPlayer::contextMenuEvent(QContextMenuEvent *event)
{
    asmenu_->clear();
    for (auto& p : decoder_->properties(AVMEDIA_TYPE_AUDIO)) {
        std::vector<std::string> title{};
        std::vector<std::string> attrs{};

        if (p.contains("language")) title.push_back(p["language"]);
        if (p.contains("title")) title.push_back(p["title"]);

        if (p.contains("Sample Format")) attrs.push_back(p["Sample Format"]);
        if (p.contains("Channel Layout")) attrs.push_back(p["Channel Layout"]);
        if (p.contains("Sample Rate")) attrs.push_back(p["Sample Rate"]);
        if (p.contains("Bitrate") && p["Bitrate"] != "N/A") attrs.push_back(p["Bitrate"]);

        std::string name = fmt::format("{} - {}", fmt::join(title, ", "), fmt::join(attrs, ", "));

        asgroup_->addAction(asmenu_->addAction(name.c_str()))->setCheckable(true);
    }

    ssmenu_->clear();
    for (auto& p : decoder_->properties(AVMEDIA_TYPE_SUBTITLE)) {
        std::vector<std::string> title{};
        std::vector<std::string> attrs{};

        if (p.contains("Codec")) attrs.push_back(p["Codec"]);

        if (p.contains("language")) title.push_back(p["language"]);
        if (p.contains("title")) title.push_back(p["title"]);

        std::string name = fmt::format("{} - {}", fmt::join(attrs, ", "), fmt::join(title, ", "));
        ssgroup_->addAction(ssmenu_->addAction(name.c_str()))->setCheckable(true);
    }

    menu_->exec(event->globalPos());
}

void VideoPlayer::showProperties()
{
    assert(decoder_);

    auto fp = decoder_->properties(AVMEDIA_TYPE_UNKNOWN);
    auto vp = decoder_->properties(AVMEDIA_TYPE_VIDEO);
    auto ap = decoder_->properties(AVMEDIA_TYPE_AUDIO);
    auto sp = decoder_->properties(AVMEDIA_TYPE_SUBTITLE);

    const auto win = new QWidget(this, Qt::Dialog);
    win->setMinimumSize(600, 800);
    win->setAttribute(Qt::WA_DeleteOnClose);
    win->setWindowTitle(tr("Properties"));
    win->setStyleSheet("QWidget { background: white; color: black; } QLabel { padding-left: 2em; }");

    const auto layout = new QVBoxLayout();
    layout->setContentsMargins({});
    win->setLayout(layout);

    const auto view = new QTreeView();
    view->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    view->setHeaderHidden(true);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(view);

    const auto model = new QStandardItemModel();

    for (size_t i = 0; i < fp.size(); ++i) {
        auto item = new QStandardItem(QString{ "General #%1" }.arg(i + 1));
        model->appendRow({ item, new QStandardItem() });

        for (const auto& [k, v] : fp[i]) {
            item->appendRow({ new QStandardItem(k.c_str()), new QStandardItem(v.c_str()) });
        }
    }

    for (size_t i = 0; i < vp.size(); ++i) {
        auto item = new QStandardItem(QString{ "Video #%1" }.arg(i + 1));
        model->appendRow({ item, new QStandardItem() });

        for (const auto& [k, v] : vp[i]) {
            item->appendRow({ new QStandardItem(k.c_str()), new QStandardItem(v.c_str()) });
        }
    }

    for (size_t i = 0; i < ap.size(); ++i) {
        auto item = new QStandardItem(QString{ "Audio #%1" }.arg(i + 1));
        model->appendRow({ item, new QStandardItem() });

        for (const auto& [k, v] : ap[i]) {
            item->appendRow({ new QStandardItem(k.c_str()), new QStandardItem(v.c_str()) });
        }
    }

    for (size_t i = 0; i < sp.size(); ++i) {
        auto item = new QStandardItem(QString{ "Subtitle #%1" }.arg(i + 1));
        model->appendRow({ item, new QStandardItem() });

        for (const auto& [k, v] : sp[i]) {
            item->appendRow({ new QStandardItem(k.c_str()), new QStandardItem(v.c_str()) });
        }
    }

    view->setModel(model);
    view->expandAll();

    win->show();
}