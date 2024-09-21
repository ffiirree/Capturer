#include "screenrecorder.h"

#include "config.h"
#include "libcap/devices.h"
#include "libcap/dispatcher.h"
#include "libcap/encoder.h"
#include "logging.h"
#include "message.h"
#include "platforms/window-effect.h"

#include <fmt/core.h>
#include <QDateTime>
#include <QMouseEvent>
#include <QStandardPaths>
#include <QTimer>

#if _WIN32

#include "libcap/win-wasapi/wasapi-capturer.h"
#include "libcap/win-wgc/wgc-capturer.h"

using DesktopCapturer = WindowsGraphicsCapturer;
using AudioCapturer   = WasapiCapturer;

#elif __linux__

#include "libcap/linux-pulse/pulse-capturer.h"
#include "libcap/linux-x/xshm-capturer.h"

using DesktopCapturer = XshmCapturer;
using AudioCapturer   = PulseCapturer;

#endif

static std::pair<AVPixelFormat, AVHWDeviceType>
set_pix_fmt(const std::unique_ptr<ScreenCapturer>&      producer,
            const std::unique_ptr<Consumer<av::frame>>& consumer, const AVCodec *vcodec)
{
    if (!vcodec) return {};

    const AVCodecHWConfig *config = nullptr;
    for (int i = 0; (config = avcodec_get_hw_config(vcodec, i)); ++i) {
        if (config->pix_fmt != AV_PIX_FMT_NONE) {

            for (const auto& producer_fmt : producer->video_formats()) {
                if (producer_fmt.hwaccel == config->device_type &&
                    producer_fmt.pix_fmt == config->pix_fmt) {

                    producer->vfmt.pix_fmt = config->pix_fmt;
                    consumer->vfmt.hwaccel = config->device_type;

                    return { config->pix_fmt, config->device_type };
                }
            }
        }
    }

    // software
    for (const auto& fmt : producer->video_formats()) {
        if (fmt.hwaccel == AV_HWDEVICE_TYPE_NONE) producer->vfmt.pix_fmt = fmt.pix_fmt;
    }
    //
    return {};
}

ScreenRecorder::ScreenRecorder(const int type, QWidget *parent)
    : QWidget(parent,
              Qt::Tool | Qt::FramelessWindowHint | Qt::BypassWindowManagerHint | Qt::WindowStaysOnTopHint),
      rec_type_(type)
{
    setAttribute(Qt::WA_TranslucentBackground);

    selector_ = new Selector(this);
    selector_->setMinValidSize(128, 128);
#ifdef _WIN32
    selector_->scope(Selector::scope_t::display);
#endif

    // recording menu
    m_mute_ = !config::recording::video::mic_enabled;
    s_mute_ = !config::recording::video::speaker_enabled;
    menu_   = new RecordingMenu(m_mute_, s_mute_, (type == GIF ? 0x00 : RecordingMenu::AUDIO), this);
    connect(menu_, &RecordingMenu::stopped, this, &ScreenRecorder::stop);
    connect(menu_, &RecordingMenu::muted, this, &ScreenRecorder::mute);
    connect(menu_, &RecordingMenu::paused, [this] {
        if (dispatcher_) dispatcher_->pause();
    });
    connect(menu_, &RecordingMenu::resumed, [this] {
        if (dispatcher_) dispatcher_->resume();
    });

    // update time of the menu
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, [this] {
        if (dispatcher_) menu_->time(av::clock::s(dispatcher_->escaped()));
    });
}

void ScreenRecorder::mute(const int type, const bool v)
{
    switch (type) {
    case 1:
        m_mute_ = v;
        if (mic_src_) mic_src_->mute(v);
        break;
    case 2:
        s_mute_ = v;
        if (speaker_src_) speaker_src_->mute(v);
        break;
    default: break;
    }
}

void ScreenRecorder::record() { !recording_ ? start() : stop(); }

constexpr auto GIF_FILTERS =
    "[0:v] split [a][b];[a] palettegen=stats_mode=single:max_colors={} [p];[b][p] paletteuse=new=1";

void ScreenRecorder::start()
{
    recording_ = true;

    filename_ = "Capturer_" + QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss_zzz").toStdString();
    if (rec_type_ == VIDEO) {
        pix_fmt_                  = config::recording::video::v::pix_fmt;
        codec_name_               = config::recording::video::v::codec;
        filters_                  = "";
        encoder_options_["vsync"] = av::to_string(av::vsync_t::cfr);

        filename_ = config::recording::video::path.toStdString() + "/" + filename_ + "." +
                    config::recording::video::mcf.toStdString();
    }
    else {
        pix_fmt_    = AV_PIX_FMT_PAL8;
        codec_name_ = "gif";
        filters_    = fmt::format(GIF_FILTERS, config::recording::gif::colors);
        if (!config::recording::gif::dither) filters_ += ":dither=none";

        filename_ = config::recording::gif::path.toStdString() + "/" + filename_ + ".gif";
    }

    // window selector
    selector_->start(probe::graphics::window_filter_t::visible |
                     probe::graphics::window_filter_t::capturable);

    setGeometry(probe::graphics::virtual_screen_geometry());
    selector_->setGeometry(rect());
    selector_->coordinate(geometry());

    selector_->show();
    show();
    TransparentInput(this, false);
    activateWindow();
}

void ScreenRecorder::setup()
{
    selector_->status(SelectorStatus::LOCKED);

    QRect region = selector_->selected(selector_->scope() != Selector::scope_t::desktop);

    menu_->disable_mic(true);
    menu_->disable_speaker(true);

    // sources & dispatcher & encoder
    desktop_src_ = std::make_unique<DesktopCapturer>();
    dispatcher_  = std::make_unique<Dispatcher>();
    encoder_     = std::make_unique<Encoder>();

    switch (rec_type_) {
    case GIF:
        desktop_src_->vfmt.framerate = config::recording::gif::framerate;
        desktop_src_->draw_cursor    = config::recording::gif::capture_mouse;
        desktop_src_->show_region    = config::recording::gif::show_region;
        encoder_->vfmt.framerate     = config::recording::gif::framerate;
        break;

    default:
        desktop_src_->vfmt.framerate = config::recording::video::v::framerate;
        desktop_src_->draw_cursor    = config::recording::video::capture_mouse;
        desktop_src_->show_region    = config::recording::video::show_region;
        encoder_->vfmt.framerate     = config::recording::video::v::framerate;
        break;
    }

    desktop_src_->show_region ? selector_->showRegion() : hide();

    TransparentInput(this, true);

    std::string name{};

#ifdef __linux__
    // https://askubuntu.com/questions/432255/what-is-the-display-environment-variable/432257#432257
    // echo $DISPLAY
    // hostname:D.S means screen S on display D of host hostname;
    name                      = fmt::format("{}.0", getenv("DISPLAY"));
    desktop_src_->level       = CAPTURE_DESKTOP;
    desktop_src_->left        = region.x();
    desktop_src_->top         = region.y();
    desktop_src_->vfmt.width  = region.width();
    desktop_src_->vfmt.height = region.height();
#elif _WIN32
    // TODO:
    //   1. rectangle mode: OK, use display mode
    //   2. widget    mode: NO, use display mode
    //   3. window    mode: OK
    //   4. display   mode: OK
    //   5. desktop   mode: NO, not supported
    switch (selector_->prey().type) {
    case hunter::prey_type_t::rectangle:
    case hunter::prey_type_t::widget:    {
        desktop_src_->level = CAPTURE_DISPLAY;
        const auto& display = probe::graphics::display_contains(selector_->selected().center()).value();

        desktop_src_->left        = region.x();
        desktop_src_->top         = region.y();
        desktop_src_->vfmt.width  = region.width();
        desktop_src_->vfmt.height = region.height();
        desktop_src_->handle      = display.handle;
        desktop_src_->show_region = false;
        break;
    }
    case hunter::prey_type_t::window:
        desktop_src_->level  = CAPTURE_WINDOW;
        desktop_src_->handle = selector_->prey().handle;
        selector_->hide();
        break;
    case hunter::prey_type_t::display:
        desktop_src_->level  = CAPTURE_DISPLAY;
        desktop_src_->handle = selector_->prey().handle;
        selector_->hide();
        break;
    default: loge("unsupported mode"); return;
    }
#endif

    // video source
    if (desktop_src_->open(name, {}) < 0) {
        Message::error(tr("Could not capture the given window"));
        loge("[RECORDER] failed to open the desktop capturer: {}", name);
        desktop_src_ = std::make_unique<DesktopCapturer>();
        stop();
        return;
    }

    dispatcher_->add_input(desktop_src_.get());

    // audio sources
    int nb_ainputs = 0;
    if (rec_type_ == VIDEO) {
        mic_src_     = std::make_unique<AudioCapturer>();
        speaker_src_ = std::make_unique<AudioCapturer>();

        if (mic_src_->open(config::devices::mic, {}) >= 0) {
            menu_->disable_mic(false);
            mic_src_->mute(m_mute_);
            dispatcher_->add_input(mic_src_.get());
            nb_ainputs++;
        }

        if (speaker_src_->open(config::devices::speaker, {}) >= 0) {
            menu_->disable_speaker(false);
            speaker_src_->mute(s_mute_);
            dispatcher_->add_input(speaker_src_.get());
            nb_ainputs++;
        }
    }

    // set hwaccel & pixel format if any
    auto [pix_fmt, hwaccel] =
        set_pix_fmt(desktop_src_, encoder_, avcodec_find_encoder_by_name(codec_name_.c_str()));

    if (hwaccel != AV_HWDEVICE_TYPE_NONE) {
        pix_fmt_ = pix_fmt;
    }

    encoder_->vfmt.pix_fmt = pix_fmt_;
    if (rec_type_ == VIDEO) {
        encoder_->vfmt.color.space = config::recording::video::v::color_space;
        encoder_->vfmt.color.range = config::recording::video::v::color_range;
    }
    encoder_->afmt.sample_fmt     = AV_SAMPLE_FMT_FLTP;
    encoder_->afmt.channels       = config::recording::video::a::channels;
    encoder_->afmt.channel_layout = av::default_channel_layout(config::recording::video::a::channels);
    encoder_->afmt.sample_rate    = config::recording::video::a::sample_rate;
    encoder_->vfmt.hwaccel        = hwaccel;

    // outputs
    dispatcher_->set_output(encoder_.get());

    // dispatcher
    dispatcher_->set_hwaccel(hwaccel);
    // TODO: the amix may not be closed with duration=longest
    const auto afilters = nb_ainputs > 1 ? fmt::format("amix=inputs={}:duration=first", nb_ainputs) : "";
    if (dispatcher_->initialize(filters_, afilters) < 0) {
        loge("create filters failed");
        stop();
        return;
    }

    encoder_options_["crf"]     = std::to_string(config::recording::video::v::crf);
    encoder_options_["vcodec"]  = codec_name_;
    encoder_options_["acodec"]  = config::recording::video::a::codec;
    encoder_options_["preset"]  = config::recording::video::v::preset;
    encoder_options_["profile"] = config::recording::video::v::profile;
    encoder_options_["tune"]    = config::recording::video::v::tune;

    if (encoder_->open(filename_, encoder_options_) < 0) {
        loge("open encoder failed");
        Message::error(tr("Could not open the encoder"));
        stop();
        return;
    }

    // start
    if (dispatcher_->start()) {
        logw("RECORDING!! Please exit first.");
        stop();
        return;
    }

    if ((rec_type_ == VIDEO && config::recording::video::floating_menu) ||
        (rec_type_ == GIF && config::recording::gif::floating_menu))
        menu_->start();
    timer_->start(50);
}

void ScreenRecorder::stop()
{
    selector_->close();
    menu_->close();

    dispatcher_  = {};
    mic_src_     = {};
    speaker_src_ = {};
    desktop_src_ = {};
    encoder_     = {};

    if (timer_->isActive()) {
        emit saved(QString::fromStdString(filename_));
        timer_->stop();
    }

    recording_ = false;

    QWidget::close();
}

void ScreenRecorder::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        stop();
    }

    if (event->key() == Qt::Key_Return) {
        setup();
    }
}

void ScreenRecorder::setStyle(const SelectorStyle& style)
{
    selector_->setBorderStyle(QPen{
        style.border_color,
        static_cast<qreal>(style.border_width),
        style.border_style,
    });

    selector_->setMaskStyle(style.mask_color);
}