#include "video-player.h"

#include "libcap/devices.h"
#include "libcap/sonic.h"
#include "logging.h"
#include "probe/defer.h"
#include "widgets/message.h"

#include <fmt/chrono.h>
#include <fmt/ranges.h>
#include <probe/graphics.h>
#include <probe/thread.h>
#include <QActionGroup>
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QMimeData>
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
    : FramelessWindow(parent, Qt::WindowMinMaxButtonsHint | Qt::WindowFullscreenButtonHint)
{
#ifndef _DEBUG
    setWindowFlag(Qt::WindowStaysOnTopHint, true);
#endif
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_TranslucentBackground); // FIXME: otherwise, the screen will be black when full
                                                // screen on Linux

    setMouseTracking(true);
    setAcceptDrops(true);

    const auto stacked_layout = new QStackedLayout(this);
    stacked_layout->setStackingMode(QStackedLayout::StackAll);
    setLayout(stacked_layout);

    // control bar
    control_ = new ControlWidget(this);

    // clang-format off
    connect(control_,   &ControlWidget::pause,          this, &VideoPlayer::pause);
    connect(control_,   &ControlWidget::resume,         this, &VideoPlayer::resume);
    connect(control_,   &ControlWidget::seek,           this, &VideoPlayer::seek);
    connect(control_,   &ControlWidget::hwToggled,      this, &VideoPlayer::hwaccel);
    connect(control_,   &ControlWidget::hdrToggled,     this, [=, this](const bool hdr){ texture_->hdr(hdr); });
    connect(control_,   &ControlWidget::speedChanged,   this, &VideoPlayer::setSpeed);
    connect(control_,   &ControlWidget::volumeChanged,  [this](auto val) { audio_renderer_->set_volume(val / 100.0f); });
    connect(control_,   &ControlWidget::mute,           [this](auto muted) { audio_renderer_->mute(muted); });
    connect(control_,   &ControlWidget::subtitlesEnabled, [this](auto en) { subtitles_enabled_ = en; if(!en) texture_->present({}, 2); });
    connect(this,       &VideoPlayer::timeChanged,      control_, &ControlWidget::setTime, Qt::QueuedConnection);
    connect(this,       &VideoPlayer::videoFinished,    this, &VideoPlayer::finish, Qt::QueuedConnection);
    connect(this,       &VideoPlayer::audioFinished,    this, &VideoPlayer::finish, Qt::QueuedConnection);
    // clang-format on
    stacked_layout->addWidget(control_);

    // decoding
    source_         = std::make_unique<Decoder>();
    // audio sink
    audio_renderer_ = std::make_unique<AudioOutput>();
    // video sink
    texture_        = new TextureRhiWidget();

    stacked_layout->addWidget(texture_);

    // hide / show toolbar
    setAttribute(Qt::WA_Hover);

    timer_ = new QTimer(this);
    timer_->setSingleShot(true);
    timer_->start(2000ms);
    connect(timer_, &QTimer::timeout, [this] {
        if (control_->hideable()) {
            setCursor(Qt::BlankCursor);
            if (control_->isVisible()) control_->hide();
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

int VideoPlayer::open(const std::string& filename)
{
    filename_ = filename;
    const QFileInfo file(QString::fromStdString(filename_));

    if (QString("GIF;APNG;WebP").contains(file.suffix(), Qt::CaseInsensitive)) {
        control_->setPlaybackMode(PlaybackMode::ANIMATED_IMAGE);
    }

    // search external subtitles
    const auto dir  = file.absoluteDir();
    auto       list = dir.entryInfoList(QStringList{ "*.ass", "*.ssa", "*.srt" });
    for (const auto& info : list) {
        if (file.completeBaseName().toLower() == info.completeBaseName().toLower()) {
            external_subtitles_.push_back(info.absoluteFilePath().toStdString());
        }
    }

    if (external_subtitles_.empty()) {
        const QRegularExpression re("S(\\d+)E(\\d+)", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch  smatch = re.match(file.completeBaseName());
        if (smatch.hasMatch()) {
            const auto captured = smatch.captured(0);

            for (const auto& info : list) {
                QRegularExpressionMatch dmatch = re.match(info.completeBaseName());
                if (dmatch.hasMatch() && captured == dmatch.captured(0)) {
                    external_subtitles_.push_back(info.absoluteFilePath().toStdString());
                }
            }
        }
    }

    // video decoder
#ifdef _WIN32
    if (control_->hwdecoded()) source_->set_hwaccel(AV_HWDEVICE_TYPE_D3D11VA, AV_PIX_FMT_D3D11);
#endif
    if (source_->open(filename) != 0) {
        source_ = std::make_unique<Decoder>();
        loge("[    PLAYER] failed to open video decoder");
        Message::error(tr("Failed to open the video decoder"));
        return -1;
    }

    if (!external_subtitles_.empty()) {
        if (source_->open_external_subtitle(external_subtitles_[0]) < 0) {
            Message::error(QString("Failed to open the external file: ") + external_subtitles_[0].c_str());
        }
    }

    control_->setDuration(av::clock::us(source_->duration()).count());

    // audio renderer
    if (source_->has(AVMEDIA_TYPE_AUDIO)) {
        audio_enabled_ = true;

        if (const auto default_asink = av::default_audio_sink();
            !default_asink ||
            audio_renderer_->open(default_asink->id, AudioRenderer::RENDER_ALLOW_STREAM_SWITCH) != 0) {
            audio_renderer_->reset();
            loge("[    PLAYER] failed to open audio render");
            Message::error(tr("Failed to open the audio output device"));
            return -1;
        }

        setVolume(static_cast<int>(audio_renderer_->volume() * 100));
        mute(audio_renderer_->muted());

        // sink audio format
        source_->afo = audio_renderer_->format();
        if (sonic_stream_ = sonic_stream_create(source_->afo.sample_fmt, source_->afo.sample_rate,
                                                source_->afo.ch_layout.nb_channels);
            !sonic_stream_) {
            loge("[    PLAYER] failed to create sonic");
            return -1;
        }
    }

    // sink video format
    if (source_->has(AVMEDIA_TYPE_VIDEO)) {
        video_enabled_       = true;
        source_->vfo         = source_->vfi;
        source_->vfo.pix_fmt = TextureRhiWidget::format(source_->vfo.pix_fmt);
    }

    control_->setHdr(source_->vfo.color.space == AVCOL_SPC_BT2020_NCL &&
                     source_->vfo.color.transfer == AVCOL_TRC_SMPTE2084);

    if (const auto stream = source_->stream(AVMEDIA_TYPE_VIDEO); stream) {
        control_->setVideoCodecName(avcodec_get_name(stream->codecpar->codec_id));
    }

    if (const auto stream = source_->stream(AVMEDIA_TYPE_AUDIO); stream) {
        control_->setAudioCodecName(avcodec_get_name(stream->codecpar->codec_id));
    }

    // title
    setWindowTitle(file.isFile() ? file.fileName() : file.filePath());

    ready_ = true;

    return 0;
}

int VideoPlayer::start()
{
    if (!ready_ || running_) {
        loge("[    PLAYER] already running or not ready");
        return -1;
    }

    source_->onarrived = [this](const av::frame& frame, auto type) { consume(frame, type); };
    if (!source_->ready() || source_->start() < 0) {
        loge("[    PLAYER] failed to start decoder");
        return -1;
    }

    running_ = true;
    timeline_.set(0ns);

    // video thread
    if (video_enabled_) video_thread_ = std::jthread([this] { video_thread_fn(); });

    // audio thread
    if (audio_enabled_) {
        audio_renderer_->callback = [this](auto ptr, auto size, auto ts) {
            return audio_callback(ptr, size, ts);
        };

        if (audio_renderer_->start() < 0) {
            loge("[    PLAYER] failed to start audio render");
            return -1;
        }
    }

    // resize & move to center of the primary screen
    const auto  screen = QApplication::primaryScreen()->geometry();
    const QSize vsize(source_->vfo.width, source_->vfo.height);
    QRect       rect({ 0, 0 }, vsize.scaled(std::min(screen.width(), vsize.width()),
                                            std::min(screen.height(), vsize.height()), Qt::KeepAspectRatio));
    rect.moveCenter(screen.center());

    setGeometry(rect);

    show();
    emit started();

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
        if (!vdone_ && vqueue_.empty() && source_->eof(AVMEDIA_TYPE_VIDEO)) {
            vdone_ = true;
            emit videoFinished();
        }

        if (vdone_ || vqueue_.stopped() || (paused() && !vstep_) || seeking_) {
            std::this_thread::sleep_for(15ms);
            continue;
        }

        const auto has_next = vqueue_.wait_and_pop();
        if (!has_next) continue;

        // subtitle
        if (subtitles_enabled_) {
            const auto [changed, sub] = source_->subtitle(timeline_.ms());
            texture_->present(sub, changed);
        }

        // video frame
        const av::frame& frame = has_next.value();
        texture_->present(frame);

        if (!vstep_ && !is_live_) {
            auto pts  = av::clock::ms(frame->pts, source_->vfo.time_base);
            auto diff = std::min((pts - timeline_.ms()) / timeline_.speed(), 300ms);

            if (diff > 5ms) std::this_thread::sleep_for(diff);
        }

        if (timeline_.time() >= 0ns) emit timeChanged(timeline_.us().count());

        vstep_ = std::max<int>(0, vstep_ - 1);
    }
}

uint32_t VideoPlayer::audio_callback(uint8_t **ptr, const uint32_t request_frames,
                                     const std::chrono::nanoseconds ts)
{
    if (seeking_ || adone_ || (paused() && !astep_)) return 0;

    if (source_->eof(AVMEDIA_TYPE_AUDIO) && aqueue_.empty()) {
        sonic_stream_flush(sonic_stream_);
        logd("flush sonic stream, remain: {}", sonic_stream_available_samples(sonic_stream_));
        if (sonic_stream_available_samples(sonic_stream_) == 0) {
            adone_ = true;
            emit audioFinished();
        }
    }
    else {
        while (!aqueue_.empty() &&
               static_cast<uint32_t>(sonic_stream_available_samples(sonic_stream_)) < request_frames * 2) {
            const auto frame = aqueue_.pop();
            if (!frame) return 0;

            // update audio clock pts
            if (frame.value()->pts >= 0)
                audio_pts_ =
                    av::clock::ns(frame.value()->pts + frame.value()->nb_samples, source_->afo.time_base);

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
        timeline_.set(audio_pts_.load() - av::clock::ns(N + M, source_->afo.time_base) * timeline_.speed(),
                      ts);
    }

    astep_ = std::max<int>(0, astep_ - 1);
    return sonic_stream_read(sonic_stream_, *ptr, request_frames);
}

int VideoPlayer::consume(const av::frame& frame, const AVMediaType type)
{
    if (source_->seeking(type)) return 0;

    if (seeking_.exchange(false)) {
        vqueue_.start();
        aqueue_.start();
    }

    if (timeline_.time() == av::clock::nopts && (frame && frame->pts >= 0)) {
        timeline_ = av::clock::ns(frame->pts, type == AVMEDIA_TYPE_VIDEO ? source_->vfo.time_base
                                                                         : source_->afo.time_base);
        logi("[{}] clock: nopts -> {:%T}", av::to_char(type), timeline_.time());
    }

    switch (type) {
    case AVMEDIA_TYPE_VIDEO:
        if (!frame || !frame->data[0]) {
            logi("[V] PLAYER INPUT EOF");
            if (vqueue_.empty()) {
                vdone_ = true;
                emit videoFinished();
            }
            return 0;
        }

        vdone_ = false;
        vqueue_.wait_and_push(frame);

        return 0;

    case AVMEDIA_TYPE_AUDIO: {
        if (!frame || !frame->data[0]) {
            logi("[A] PLAYER INPUT EOF");
            return 0;
        }

        adone_ = false;
        aqueue_.wait_and_push(frame);

        return 0;
    }

    default: return -1;
    }
}

void VideoPlayer::pause()
{
    timeline_.pause();
    paused_ = true;
}

void VideoPlayer::resume()
{
    timeline_.resume();
    paused_ = false;
}

// only called by UI thread
void VideoPlayer::seek(const std::chrono::nanoseconds ts, const std::chrono::nanoseconds rel)
{
    if (seeking_ || ts < 0s) return;

    vqueue_.stop();
    aqueue_.stop();

    source_->seek(ts, rel);

    if (sonic_stream_) sonic_stream_drain(sonic_stream_);

    // reset state
    seeking_   = true;
    timeline_  = av::clock::nopts;
    audio_pts_ = av::clock::nopts;

    audio_renderer_->reset();

    if (paused()) vstep_ = 1;
}

void VideoPlayer::hwaccel(bool en)
{
    if (en) {
#ifdef _WIN32
        source_->set_hwaccel(AV_HWDEVICE_TYPE_D3D11VA, AV_PIX_FMT_D3D11);
#endif
    }
    else {
        source_->set_hwaccel(AV_HWDEVICE_TYPE_NONE, AV_PIX_FMT_NONE);
    }
}

void VideoPlayer::finish()
{
    logi("PLAYBACK EOF: [V] {}, [A] {}, VQ: {}, AQ: {}, feof: {}", vdone_.load(), adone_.load(),
         vqueue_.size(), aqueue_.size(), source_->eof());
    if ((vdone_ && adone_) ||
        (vqueue_.empty() && aqueue_.empty() &&
         (!sonic_stream_ || sonic_stream_expected_samples(sonic_stream_) == 0) && source_->eof())) {
        logi("[    PLAYER] {} is finished", filename_);
        if (!is_live_) {
            pause();
            emit control_->pause();

            // seek to start time
            seek(0ns, 0ns);
        }
    }
}

void VideoPlayer::stop()
{
    running_ = false;
    ready_   = false;

    vqueue_.stop();
    aqueue_.stop();

    audio_renderer_->stop();

    source_->stop();

    if (video_thread_.joinable()) video_thread_.join();

    logi("[    PLAYER] [{:>10}] STOPPED", filename_);
}

VideoPlayer::~VideoPlayer()
{
    stop();

    if (sonic_stream_ != nullptr) {
        sonic_stream_destroy(sonic_stream_);
        sonic_stream_ = nullptr;
    }

    logi("[    PLAYER] [{:>10}] ~", filename_);
}

void VideoPlayer::setSpeed(const float speed)
{
    if (sonic_stream_) sonic_stream_set_speed(sonic_stream_, speed);
    timeline_.set_speed({ static_cast<intmax_t>(speed * 1000000), 1000000 });
}

void VideoPlayer::mute(const bool muted) { control_->setMute(muted); }

void VideoPlayer::setVolume(const int volume) { control_->setVolume(std::clamp<int>(volume, 0, 100)); }

bool VideoPlayer::event(QEvent *event)
{
    if (event->type() == QEvent::HoverMove) {
        timer_->start(2000ms);

        setCursor(Qt::ArrowCursor);
        if (!control_->isVisible() && !control_->hideable()) control_->show();
    }
    else if (event->type() == QEvent::Show) {
        control_->hide();
    }

    return FramelessWindow::event(event);
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

void VideoPlayer::resizeEvent(QResizeEvent *event)
{
    if (source_ && texture_) {
        const auto sz = texture_->renderSize();
        source_->set_ass_render_size(sz.width(), sz.height());
    }
    FramelessWindow::resizeEvent(event);
}

void VideoPlayer::dropEvent(QDropEvent *event)
{
    const auto urls = event->mimeData()->urls();
    if (urls.empty()) return;

    for (const auto& url : urls) {
        QFileInfo info{ url.toLocalFile() };
        if (info.isFile() && QString("ass;ssa;srt;vtt;stl;sbv;sub;dfxp;ttml")
                                 .split(';')
                                 .contains(info.suffix(), Qt::CaseInsensitive)) {
            logi("add subtitle: {}", info.fileName().toStdString());

            if (source_) {
                if (source_->open_external_subtitle(info.absoluteFilePath().toStdString()) < 0) {
                    Message::error("Failed to open the external file: " + info.absoluteFilePath());
                }
            }
        }
    }
}

void VideoPlayer::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void VideoPlayer::initContextMenu()
{
    menu_ = new Menu(this);

    // video menu
    {
        const auto menu = new Menu(tr("Video"), this);
        {
            const auto hflip = menu->addAction(tr("H Flip"), this, [=, this]() { texture_->hflip(); });
            hflip->setCheckable(true);

            const auto vflip = menu->addAction(tr("V Flip"), this, [=, this]() { texture_->vflip(); });
            vflip->setCheckable(true);

            menu->addSeparator();

            const auto group = new QActionGroup(this);

            const auto r0   = menu->addAction(tr("Rotate 0"), this, [this] { texture_->rotate(0); });
            const auto r90  = menu->addAction(tr("Rotate 90"), this, [this] { texture_->rotate(90); });
            const auto r180 = menu->addAction(tr("Rotate 180"), this, [this] { texture_->rotate(180); });
            const auto r270 = menu->addAction(tr("Rotate 270"), this, [this] { texture_->rotate(270); });

            group->addAction(r0)->setCheckable(true);
            group->addAction(r90)->setCheckable(true);
            group->addAction(r180)->setCheckable(true);
            group->addAction(r270)->setCheckable(true);

            r0->setChecked(true);
        }
        menu_->addMenu(menu);
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
#ifdef _WIN32
            group->addAction(renders->addAction("WASAPI"))->setCheckable(true);
#else
            group->addAction(renders->addAction("PulseAudio"))->setCheckable(true);
#endif
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
        subtitles_menu->addAction(tr("Add Subtitles"), this, [=, this]() {
            const auto file = QFileDialog::getOpenFileName(this, tr("Add Subtitles"),
                                                           QFileInfo{ filename_.c_str() }.absolutePath(),
                                                           "Subtitles (*.ass *.ssa *.srt)");
            if (!file.isEmpty() && source_) {
                source_->open_external_subtitle(QFileInfo(file).absoluteFilePath().toStdString());
            }
        });
        menu_->addMenu(subtitles_menu);
    }

    menu_->addSeparator();

    addAction(menu_->addAction(tr("Properties"), QKeySequence(Qt::CTRL | Qt::Key_I), this,
                               &VideoPlayer::showProperties));
}

void VideoPlayer::contextMenuEvent(QContextMenuEvent *event)
{
    asmenu_->clear();
    for (auto& p : source_->properties(AVMEDIA_TYPE_AUDIO)) {
        std::vector<std::string> title{};
        std::vector<std::string> attrs{};

        if (p.contains("language")) title.push_back(p["language"]);
        if (p.contains("title")) title.push_back(p["title"]);

        if (p.contains("Sample Format")) attrs.push_back(p["Sample Format"]);
        if (p.contains("Channel Layout")) attrs.push_back(p["Channel Layout"]);
        if (p.contains("Sample Rate")) attrs.push_back(p["Sample Rate"]);
        if (p.contains("Bitrate") && p["Bitrate"] != "N/A") attrs.push_back(p["Bitrate"]);

        std::string name =
            fmt::format("{}{}", title.empty() ? "" : fmt::format("{} - ", fmt::join(title, ", ")),
                        fmt::join(attrs, ", "));

        const auto action = asmenu_->addAction(name.c_str(), this, [=, this]() {
            source_->select(AVMEDIA_TYPE_AUDIO, probe::util::to_32i(p.at("Index")).value_or(-1));
        });
        action->setCheckable(true);
        if (p["Index"] == std::to_string(source_->index(AVMEDIA_TYPE_AUDIO))) {
            action->setChecked(true);
        }
        asgroup_->addAction(action);
    }

    ssmenu_->clear();
    for (auto& p : source_->properties(AVMEDIA_TYPE_SUBTITLE)) {
        std::vector<std::string> title{};
        std::vector<std::string> attrs{};

        if (p.contains("Codec")) attrs.push_back(p["Codec"]);

        if (p.contains("language")) title.push_back(p["language"]);
        if (p.contains("title")) title.push_back(p["title"]);

        std::string name = fmt::format("{} - {}", fmt::join(attrs, ", "), fmt::join(title, ", "));

        const auto action = ssmenu_->addAction(name.c_str(), this, [=, this]() {
            source_->select(AVMEDIA_TYPE_SUBTITLE, probe::util::to_32i(p.at("Index")).value_or(-1));
        });
        action->setCheckable(true);
        if (p["Index"] == std::to_string(source_->index(AVMEDIA_TYPE_SUBTITLE))) {
            action->setChecked(true);
        }
        ssgroup_->addAction(action);
    }

    for (auto& sub : external_subtitles_) {
        const auto action = ssmenu_->addAction(QFileInfo{ sub.c_str() }.fileName(), this,
                                               [=, this]() { source_->open_external_subtitle(sub); });
        action->setCheckable(true);
        if (source_ && source_->external_subtitle() == sub) {
            action->setChecked(true);
        }
        ssgroup_->addAction(action);
    }

    menu_->exec(event->globalPos());
}

void VideoPlayer::showProperties()
{
    assert(source_);

    auto fp = source_->properties(AVMEDIA_TYPE_UNKNOWN);
    auto vp = source_->properties(AVMEDIA_TYPE_VIDEO);
    auto ap = source_->properties(AVMEDIA_TYPE_AUDIO);
    auto sp = source_->properties(AVMEDIA_TYPE_SUBTITLE);

    const auto win = new QDialog(this);
    win->setMinimumSize(600, 800);
    win->setAttribute(Qt::WA_DeleteOnClose);
    win->setWindowTitle(tr("Properties"));

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