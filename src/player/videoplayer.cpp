#include "videoplayer.h"

#include "libcap/devices.h"
#include "libcap/sonic.h"
#include "logging.h"

#include <fmt/chrono.h>
#include <fmt/ranges.h>
#include <probe/graphics.h>
#include <probe/thread.h>
#include <QActionGroup>
#include <QApplication>
#include <QFileInfo>
#include <QHeaderView>
#include <QMouseEvent>
#include <QScreen>
#include <QShortcut>
#include <QStackedLayout>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>

#ifdef _WIN32
#include "libcap/win-wasapi/wasapi-renderer.h"
using AudioOutput = WasapiRenderer;
#elif __linux__
#include "libcap/linux-pulse/pulse-renderer.h"
using AudioOutput = PulseAudioRenderer;
#endif

VideoPlayer::VideoPlayer(QWidget *parent)
    : FramelessWindow(parent, Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint |
                                  Qt::WindowFullscreenButtonHint | Qt::WindowStaysOnTopHint)
{
    setMouseTracking(true);
    installEventFilter(this);

    setAttribute(Qt::WA_DeleteOnClose);

    const auto stacked_layout = new QStackedLayout(this);
    stacked_layout->setStackingMode(QStackedLayout::StackAll);
    setLayout(stacked_layout);

    // control bar
    control_ = new ControlWidget(this);
    // clang-format off
    connect(control_,   &ControlWidget::pause,          this, &VideoPlayer::pause);
    connect(control_,   &ControlWidget::resume,         this, &VideoPlayer::resume);
    connect(control_,   &ControlWidget::seek,           this, &VideoPlayer::seek);
    connect(control_,   &ControlWidget::speedChanged,   this, &VideoPlayer::setSpeed);
    connect(control_,   &ControlWidget::volumeChanged,  [this](auto val) { audio_renderer_->set_volume(val / 100.0f); });
    connect(control_,   &ControlWidget::mute,           [this](auto muted) { audio_renderer_->mute(muted); });
    connect(this,       &VideoPlayer::timeChanged,      control_, &ControlWidget::setTime, Qt::QueuedConnection);
    connect(this,       &VideoPlayer::videoFinished,    this, &VideoPlayer::finish, Qt::QueuedConnection);
    connect(this,       &VideoPlayer::audioFinished,    this, &VideoPlayer::finish, Qt::QueuedConnection);
    // clang-format on
    stacked_layout->addWidget(control_);

    // decoding
    decoder_        = std::make_unique<Decoder>();
    // filter graph
    dispatcher_     = std::make_unique<Dispatcher>();
    // audio sink
    audio_renderer_ = std::make_unique<AudioOutput>();
    // video sink
    texture_        = new TextureGLWidget();

    stacked_layout->addWidget(texture_);

    // hide / show tool bar
    setAttribute(Qt::WA_Hover);

    timer_ = new QTimer(this);
    timer_->setSingleShot(true);
    timer_->start(2500ms);
    connect(timer_, &QTimer::timeout, [this] {
        if (control_->isVisible() && control_->hideable()) {
            setCursor(Qt::BlankCursor);
            control_->hide();
        }
    });

    // clang-format off
    connect(new QShortcut(Qt::Key_Right, this), &QShortcut::activated, [this] { seek(timeline_.time() + 5s, + 5s); });
    connect(new QShortcut(Qt::Key_Left,  this), &QShortcut::activated, [this] { seek(timeline_.time() - 5s, - 5s); });
    connect(new QShortcut(Qt::Key_Space, this), &QShortcut::activated, [this] { control_->paused() ? control_->resume() : control_->pause(); });
    connect(new QShortcut(Qt::Key_Up,    this), &QShortcut::activated, [this] { control_->setVolume(audio_renderer_->volume() * 100 + 5); });
    connect(new QShortcut(Qt::Key_Down,  this), &QShortcut::activated, [this] { control_->setVolume(audio_renderer_->volume() * 100 - 5); });
    connect(new QShortcut(Qt::Key_F,     this), &QShortcut::activated, [this] { if(paused()) { vstep_ += 1; astep_ += 1; } });
    // clang-format on

    initContextMenu();
}

int VideoPlayer::open(const std::string& filename, std::map<std::string, std::string> options)
{
    filename_ = filename;

    std::map<std::string, std::string> decoder_options;
    if (options.contains("format")) {
        decoder_options["format"] = options.at("format");
        if (options.at("format") == "dshow" || options.at("format") == "v4l2") {
            is_live_ = true;
            control_->setLiveMode(true);
        }
    }

#if 0
    decoder_options["hwaccel"]         = "d3d11va";
    decoder_options["hwaccel_pix_fmt"] = "d3d11";
#endif

    // video decoder
    if (decoder_->open(filename, decoder_options) != 0) {
        decoder_ = std::make_unique<Decoder>();
        LOG(ERROR) << "[    PLAYER] failed to open video decoder";
        return -1;
    }

    control_->setDuration(av::clock::us(decoder_->duration()).count());

    // audio renderer
    if (const auto default_asink = av::default_audio_sink();
        !default_asink || audio_renderer_->open(default_asink->id, {}) != 0) {
        audio_renderer_->reset();
        LOG(ERROR) << "[    PLAYER] failed to open audio render";
        return -1;
    }

    setVolume(static_cast<int>(audio_renderer_->volume() * 100));
    mute(audio_renderer_->muted());

    // sink audio format
    afmt = audio_renderer_->format();
    if (sonic_stream_ = sonic_stream_create(afmt.sample_fmt, afmt.sample_rate, afmt.channels);
        !sonic_stream_) {
        LOG(ERROR) << "[    PLAYER] failed to create sonic";
        return -1;
    }

    // sink video format
    vfmt.pix_fmt = texture_->isSupported(decoder_->vfmt.pix_fmt) ? decoder_->vfmt.pix_fmt
                                                                 : TextureGLWidget::pix_fmts()[0];

    // dispatcher
    dispatcher_->add_input(decoder_.get());
    dispatcher_->set_output(this);
    if (dispatcher_->initialize((options.contains("filters") ? options.at("filters") : ""), "")) {
        LOG(ERROR) << "[    PLAYER] failed to create filter graph";
        return -1;
    }

    if (texture_->setFormat(vfmt) < 0) {
        LOG(ERROR) << "[    PLAYER] unsupported video format: " << av::to_string(vfmt);
        return -1;
    }

    ready_ = true;

    // start playback
    if (dispatcher_->start() < 0) {
        dispatcher_ = std::make_unique<Dispatcher>();
        LOG(ERROR) << "[    PLAYER] failed to start dispatcher";
        return -1;
    }

    // title
    const QFileInfo info(QString::fromStdString(filename_));
    setWindowTitle(info.isFile() ? info.fileName() : info.filePath());

    // resize & move to center
    (vfmt.width > 1920 && vfmt.height > 1080)
        ? resize(QSize(vfmt.width, vfmt.height).scaled(1920, 1080, Qt::KeepAspectRatio))
        : resize(vfmt.width, vfmt.height);

    move(QApplication::primaryScreen()->geometry().center() - QPoint(vfmt.width / 2, vfmt.height / 2));

    QWidget::show();

    emit started();

    return 0;
}

int VideoPlayer::start()
{
    if (!ready_ || running_) {
        LOG(ERROR) << "[    PLAYER] already running or not ready";
        return -1;
    }

    running_ = true;
    timeline_.set(0ns);

    // video thread
    if (video_enabled_) {
        video_thread_ = std::jthread(&VideoPlayer::video_thread_fn, this);
    }

    // audio thread
    if (audio_enabled_) {
        audio_renderer_->callback = [this](auto ptr, auto size, auto ts) {
            return audio_callback(ptr, size, ts);
        };

        if (audio_renderer_->start() < 0) {
            LOG(ERROR) << "[    PLAYER] failed to start audio render";
            return -1;
        }
    }

    return 0;
}

void VideoPlayer::video_thread_fn()
{
    probe::thread::set_name("PLAYER-VIDEO");

#ifdef _WIN32
    ::timeBeginPeriod(1);
    defer(::timeEndPeriod(1));
#endif

    while (running_) {
        if (vbuffer_.empty() && (eof_ & VINPUT_EOF) && !(eof_ & VPLAY_EOF)) {
            eof_ |= VPLAY_EOF;
            emit videoFinished();
        }

        if ((eof_ & VPLAY_EOF) || vbuffer_.stopped() || (paused() && !vstep_) || seeking_) {
            std::this_thread::sleep_for(10ms);
            continue;
        }

        const auto has_next = vbuffer_.wait_and_pop();
        if (!has_next) continue;

        const av::frame& frame = has_next.value();
        texture_->present(frame);

        if (!vstep_ && !is_live_) {
            auto pts  = av::clock::ms(frame->pts, vfmt.time_base);
            auto diff = std::min((pts - timeline_.ms()) / timeline_.speed(), 500ms);

            if (diff < 5ms) continue;

            DLOG(INFO) << fmt::format("[V] {:.3%T} ~ {:.3%T} -> {:.3%S}", pts, timeline_.time(), diff);

            std::this_thread::sleep_for(diff);
        }

        if (timeline_.time() >= 0ns) emit timeChanged(timeline_.us().count());

        vstep_ = std::max<int>(0, vstep_ - 1);
    }
}

uint32_t VideoPlayer::audio_callback(uint8_t **ptr, const uint32_t request_frames,
                                     const std::chrono::nanoseconds ts)
{
    if (seeking_ || (eof_ & APLAY_EOF) || (paused() && !astep_)) return 0;

    if ((eof_ & AINPUT_EOF) && abuffer_.empty()) {
        sonic_stream_flush(sonic_stream_);
        DLOG(INFO) << "flush sonic stream : " << sonic_stream_available_samples(sonic_stream_);
        if (sonic_stream_available_samples(sonic_stream_) == 0) {
            eof_ |= APLAY_EOF;
            emit audioFinished();
        }
    }
    else {
        while (!abuffer_.empty() &&
               static_cast<uint32_t>(sonic_stream_available_samples(sonic_stream_)) < request_frames * 2) {
            const auto frame = abuffer_.pop();
            if (!frame) return 0;

            // update audio clock pts
            if (frame.value()->pts >= 0) audio_pts_ = av::clock::ns(frame.value()->pts, afmt.time_base);

            if (audio_pts_.load() != av::clock::nopts)
                audio_pts_ = audio_pts_.load() + av::clock::ns(frame.value()->nb_samples, afmt.time_base);

            sonic_stream_write(sonic_stream_, frame.value()->data[0], frame.value()->nb_samples);
        }
    }

    // | renderer buffered samples |  sonic samples  |  decoding
    //                                               ^
    //                                               |
    //                                           audio pts
    if (audio_pts_.load() != av::clock::nopts) {
        const uint32_t N = sonic_stream_expected_samples(sonic_stream_);    // sonic
        const uint32_t M = audio_renderer_->buffer_size() - request_frames; // renderer
        timeline_.set(audio_pts_.load() - av::clock::ns(N + M, afmt.time_base) * timeline_.speed(), ts);
    }

    astep_ = std::max<int>(0, astep_ - 1);
    return sonic_stream_read(sonic_stream_, *ptr, request_frames);
}

int VideoPlayer::consume(const av::frame& frame, const AVMediaType type)
{
    if (seeking_) {
        seeking_ = false;
        vbuffer_.start();
        abuffer_.start();
    }

    if (timeline_.time() == av::clock::nopts && frame->pts >= 0) {
        timeline_ = av::clock::ns(frame->pts, type == AVMEDIA_TYPE_VIDEO ? vfmt.time_base : afmt.time_base);
        LOG(INFO) << fmt::format("[{}] clock: nopts -> {:%T}", av::to_char(type), timeline_.time());
    }

    switch (type) {
    case AVMEDIA_TYPE_VIDEO:
        if (!frame || !frame->data[0]) {
            eof_ |= VINPUT_EOF;
            LOG(INFO) << "[V] PLAYER INPUT EOF";

            if (vbuffer_.empty()) {
                eof_ |= VPLAY_EOF;
                emit videoFinished();
            }
            return 0;
        }

        eof_ &= ~(VINPUT_EOF | VPLAY_EOF);
        vbuffer_.wait_and_push(frame);

        return 0;

    case AVMEDIA_TYPE_AUDIO: {
        if (!frame || !frame->data[0]) {
            eof_ |= AINPUT_EOF;
            LOG(INFO) << "[A] PLAYER INPUT EOF";
            return 0;
        }

        eof_ &= ~(AINPUT_EOF | APLAY_EOF);
        abuffer_.wait_and_push(frame);

        return 0;
    }

    default: return -1;
    }
}

void VideoPlayer::enable(const AVMediaType type, const bool v)
{
    // clang-format off
    switch (type) {
    case AVMEDIA_TYPE_VIDEO: video_enabled_  = v; break;
    case AVMEDIA_TYPE_AUDIO: audio_enabled_  = v; break;
    default: break;
    }
    // clang-format on
}

bool VideoPlayer::accepts(const AVMediaType type) const
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO: return video_enabled_;
    case AVMEDIA_TYPE_AUDIO: return audio_enabled_;
    default:                 return false;
    }
}

void VideoPlayer::pause()
{
    timeline_.pause();
    // audio_renderer_->pause();
    paused_ = true;
}

void VideoPlayer::resume()
{
    timeline_.resume();
    paused_ = false;
    // audio_renderer_->resume();
}

// only called by UI thread
void VideoPlayer::seek(const std::chrono::nanoseconds ts, const std::chrono::nanoseconds rel)
{
    if (seeking_ || ts < 0s) return;

    // stop
    vbuffer_.stop();
    abuffer_.stop();

    vbuffer_.drain();
    abuffer_.drain();

    sonic_stream_drain(sonic_stream_);

    // seek
    dispatcher_->seek(ts, rel);

    // reset state
    seeking_   = true;
    eof_       = 0x00;
    timeline_  = av::clock::nopts;
    audio_pts_ = av::clock::nopts;

    audio_renderer_->reset();

    if (paused()) vstep_ = 1;
}

void VideoPlayer::finish()
{
    LOG(INFO) << fmt::format("PLAYBACK EOF: [V] {}, [A] {}", !!(eof_ & VPLAY_EOF), !!(eof_ & APLAY_EOF));
    if (eof()) {
        LOG(INFO) << "[    PLAYER] \"" << filename_ << "\" is finished";
        if (!is_live_) {
            pause();
            emit control_->pause();

            // seek to start time
            seek(0ns, 0ns);
        }
    }
}

bool VideoPlayer::eof() const
{
    return (!video_enabled_ || (eof_ & VPLAY_EOF)) && (!audio_enabled_ || (eof_ & APLAY_EOF));
}

void VideoPlayer::stop()
{
    running_ = false;
    ready_   = false;

    audio_renderer_->stop();

    // drain & skip the waited frames @{
    vbuffer_.stop();
    abuffer_.stop();
    vbuffer_.drain();
    abuffer_.drain();
    // @}

    if (video_thread_.joinable()) video_thread_.join();

    LOG(INFO) << fmt::format("[    PLAYER] [{:>10}] STOPPED", filename_);
}

VideoPlayer::~VideoPlayer()
{
    dispatcher_ = {};

    if (sonic_stream_ != nullptr) {
        sonic_stream_destroy(sonic_stream_);
        sonic_stream_ = nullptr;
    }

    LOG(INFO) << fmt::format("[    PLAYER] [{:>10}] ~", filename_);
}

void VideoPlayer::setSpeed(const float speed)
{
    sonic_stream_set_speed(sonic_stream_, speed);
    timeline_.set_speed({ static_cast<intmax_t>(speed * 1000000), 1000000 });
}

void VideoPlayer::mute(const bool muted) { control_->setMute(muted); }

void VideoPlayer::setVolume(const int volume) { control_->setVolume(std::clamp<int>(volume, 0, 100)); }

bool VideoPlayer::event(QEvent *event)
{
    if (event->type() == QEvent::HoverMove) {
        timer_->start(2500ms);

        if (control_ && !control_->isVisible()) {
            setCursor(Qt::ArrowCursor);
            control_->show();
        }
    }

    return FramelessWindow::event(event);
}

void VideoPlayer::closeEvent(QCloseEvent *event)
{
    if (dispatcher_) dispatcher_->stop();

    FramelessWindow::closeEvent(event);
}

void VideoPlayer::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::BackButton) {
        seek(timeline_.time() - 5s, -5s);
    }
    else if (event->button() == Qt::ForwardButton) {
        seek(timeline_.time() + 5s, +5s);
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