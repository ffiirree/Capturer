#include "screenrecorder.h"

#include "config.h"
#include "libcap/devices.h"
#include "libcap/dispatcher.h"
#include "libcap/encoder.h"
#include "logging.h"

#include <fmt/core.h>
#include <QApplication>
#include <QDateTime>
#include <QMouseEvent>
#include <QStandardPaths>

#if _WIN32

#include "libcap/win-wasapi/wasapi-capturer.h"
#include "libcap/win-wgc/wgc-capturer.h"

using DesktopCapturer = WindowsGraphicsCapturer;
using AudioCapturer   = WasapiCapturer;

#elif __linux__

#include "libcap/linux-pulse/pulse-capturer.h"

using DesktopCapturer = Decoder;
using AudioCapturer   = PulseCapturer;

#endif

static std::pair<AVPixelFormat, AVHWDeviceType>
set_pix_fmt(const std::unique_ptr<Producer<av::frame>>& producer,
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

ScreenRecorder::ScreenRecorder(int type, QWidget *parent)
    : QWidget(parent), rec_type_(type)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::BypassWindowManagerHint |
                   Qt::WindowStaysOnTopHint);

    selector_ = new Selector(this);
    selector_->setMinValidSize(128, 128);
#ifdef _WIN32
    selector_->scope(Selector::scope_t::display);
#endif

    // recording menu
    menu_ = new RecordingMenu(m_mute_, s_mute_, (type == GIF ? 0x00 : RecordingMenu::AUDIO), this);
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

void ScreenRecorder::mute(int type, bool v)
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

void ScreenRecorder::start()
{
    recording_ = true;

    auto time_str = QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss_zzz").toStdString();
    if (rec_type_ == VIDEO) {
        pix_fmt_                  = AV_PIX_FMT_YUV420P;
        codec_name_               = Config::instance()["record"]["encoder"];
        filters_                  = "";
        encoder_options_["vsync"] = av::to_string(av::vsync_t::cfr);

        auto root_dir = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation).toStdString();
        filename_     = fmt::format("{}/Capturer_{}.mp4", root_dir, time_str);
    }
    else {
        pix_fmt_    = AV_PIX_FMT_PAL8;
        codec_name_ = "gif";
        filters_    = gif_filters_[Config::instance()["gif"]["quality"]];

        auto root_dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation).toStdString();
        filename_     = fmt::format("{}/Capturer_{}.gif", root_dir, time_str);
    }

    // window selector
    selector_->start(probe::graphics::window_filter_t::visible |
                     probe::graphics::window_filter_t::capturable);

    setGeometry(probe::graphics::virtual_screen_geometry());
    selector_->setGeometry(rect());
    selector_->coordinate(geometry());

    selector_->show();
    show();
    activateWindow();
}

void ScreenRecorder::setup()
{
    selector_->status(SelectorStatus::LOCKED);

    QRect region = selector_->selected(selector_->scope() != Selector::scope_t::desktop);

    const auto show_region = Config::instance()[rec_type_ == VIDEO ? "record" : "gif"]["box"].get<bool>();
#ifdef _WIN32
    show_region && (selector_->prey().type != hunter::prey_type_t::window &&
                    selector_->prey().type != hunter::prey_type_t::display)
        ? selector_->showRegion()
        : hide();
#else
    show_region ? selector_->showRegion() : hide();
#endif

    menu_->disable_mic(true);
    menu_->disable_speaker(true);

    // desktop capturer
    framerate_ = Config::instance()[rec_type_ == VIDEO ? "record" : "gif"]["framerate"].get<int>();
    const auto draw_mouse = Config::instance()[rec_type_ == VIDEO ? "record" : "gif"]["mouse"].get<bool>();

    std::string                        name{};
    std::map<std::string, std::string> options{
        { "framerate", std::to_string(framerate_) },
        { "video_size", fmt::format("{}x{}", (region.width() / 2) * 2, (region.height() / 2) * 2) },
        { "draw_mouse", std::to_string(static_cast<int>(draw_mouse)) },
    };

    options["video_size"] = fmt::format("{}x{}", (region.width() / 2) * 2, (region.height() / 2) * 2);

#ifdef __linux__
    // https://askubuntu.com/questions/432255/what-is-the-display-environment-variable/432257#432257
    // echo $DISPLAY
    name              = fmt::format("{}.0+{},{}", getenv("DISPLAY"), region.x(), region.y());
    options["format"] = "x11grab";
#elif _WIN32
    // TODO:
    //   1. rectanle mode: OK, use display mode
    //   2. widget   mode: NO, use display mode
    //   3. window   mode: OK
    //   4. display  mode: OK
    //   5. desktop  mode: NO, not supported
    options["show_region"] = show_region ? "true" : "false";
    switch (selector_->prey().type) {
    case hunter::prey_type_t::rectangle:
    case hunter::prey_type_t::widget:    {
        options["show_region"] = "false";
        options["offset_x"]    = std::to_string(region.x());
        options["offset_y"]    = std::to_string(region.y());
        auto display           = probe::graphics::display_contains(selector_->selected().center()).value();
        name                   = "display=" + std::to_string(display.handle);
        break;
    }
    case hunter::prey_type_t::window:  name = "window=" + std::to_string(selector_->prey().handle); break;
    case hunter::prey_type_t::display: name = "display=" + std::to_string(selector_->prey().handle); break;
    default:                           LOG(ERROR) << "unsuppored mode"; return;
    }
#endif

    // sources & dispatcher & encoder
    if (rec_type_ == VIDEO) {
        mic_src_     = std::make_unique<AudioCapturer>();
        speaker_src_ = std::make_unique<AudioCapturer>();
    }

    desktop_src_ = std::make_unique<DesktopCapturer>();
    dispatcher_  = std::make_unique<Dispatcher>();
    encoder_     = std::make_unique<Encoder>();

    // video source
    if (desktop_src_->open(name, options) < 0) {
        LOG(ERROR) << fmt::format("[RECORDER] failed to open the desktop capturer: {}", name);
        desktop_src_ = std::make_unique<DesktopCapturer>();
        stop();
        return;
    }

    dispatcher_->add_input(desktop_src_.get());

    int nb_ainputs = 0;

    // audio source
    if (rec_type_ == VIDEO) {
        if (!av::audio_sources().empty() && mic_src_) {
            if (mic_src_->open(Config::instance()["devices"]["microphones"].get<std::string>(), {}) < 0) {
                LOG(WARNING) << "open microphone failed";
                mic_src_ = {};
            }
            else if (mic_src_->ready()) {
                menu_->disable_mic(false);
                mic_src_->mute(m_mute_);
                dispatcher_->add_input(mic_src_.get());
                nb_ainputs++;
            }
        }

        if (!av::audio_sinks().empty() && speaker_src_) {
            if (speaker_src_->open(Config::instance()["devices"]["speakers"].get<std::string>(), {}) < 0) {
                LOG(WARNING) << "open speaker failed";
                speaker_src_ = {};
            }
            else if (speaker_src_->ready()) {
                menu_->disable_speaker(false);
                speaker_src_->mute(s_mute_);
                dispatcher_->add_input(speaker_src_.get());
                nb_ainputs++;
            }
        }
    }

    // set hwaccel & pixel format if any
    auto [pix_fmt, hwaccel] =
        set_pix_fmt(desktop_src_, encoder_, avcodec_find_encoder_by_name(codec_name_.c_str()));

    std::string quality_name = "crf"; // TODO: CRF / CQP
    if (hwaccel != AV_HWDEVICE_TYPE_NONE) {
        quality_name = "cq";
        pix_fmt_     = pix_fmt;
    }

    encoder_->vfmt.pix_fmt        = pix_fmt_;
    encoder_->vfmt.framerate      = { framerate_, 1 };
    encoder_->afmt.sample_fmt     = AV_SAMPLE_FMT_FLTP;
    encoder_->afmt.channels       = 2;
    encoder_->afmt.channel_layout = AV_CH_LAYOUT_STEREO;
    encoder_->afmt.sample_rate    = 48000;
    encoder_->vfmt.hwaccel        = hwaccel;

    // outputs
    dispatcher_->set_output(encoder_.get());

    // dispatcher
    dispatcher_->set_hwaccel(hwaccel);
    // TODO: the amix may not be closed with duration=longest
    const auto afilters = nb_ainputs > 1 ? fmt::format("amix=inputs={}:duration=first", nb_ainputs) : "";
    if (dispatcher_->initialize(filters_, afilters) < 0) {
        LOG(INFO) << "create filters failed";
        stop();
        return;
    }

    encoder_options_[quality_name] = video_qualities_[Config::instance()["record"]["quality"]];
    encoder_options_["vcodec"]     = codec_name_;
    encoder_options_["acodec"]     = "aac";

    if (encoder_->open(filename_, encoder_options_) < 0) {
        LOG(INFO) << "open encoder failed";
        stop();
        return;
    }

    // start
    if (dispatcher_->start()) {
        LOG(WARNING) << "RECORDING!! Please exit first.";
        stop();
        return;
    }

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
        emit SHOW_MESSAGE(rec_type_ == VIDEO ? "Capturer<VIDEO>" : "Capturer<GIF>",
                          tr("Path: ") + QString::fromStdString(filename_));
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

void ScreenRecorder::updateTheme()
{
    const auto& style = Config::instance()[rec_type_ == VIDEO ? "record" : "gif"]["selector"];
    selector_->setBorderStyle(QPen{
        style["border"]["color"].get<QColor>(),
        static_cast<qreal>(style["border"]["width"].get<int>()),
        style["border"]["style"].get<Qt::PenStyle>(),
    });

    selector_->setMaskStyle(style["mask"]["color"].get<QColor>());
}