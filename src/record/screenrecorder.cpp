#include "screenrecorder.h"

#include "config.h"
#include "devices.h"
#include "logging.h"

#include <fmt/core.h>
#include <QApplication>
#include <QDateTime>
#include <QMouseEvent>
#include <QPushButton>
#include <QStandardPaths>
extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavutil/pixdesc.h>
}
#ifdef __linux__
#include <cstdlib>
#endif

ScreenRecorder::ScreenRecorder(int type, QWidget *parent) : Selector(parent)
{
    windows_detection_flags_ =
        probe::graphics::window_filter_t::visible | probe::graphics::window_filter_t::capturable;

#ifdef _WIN32
    scope_ = scope_t::display;
#endif

    recording_type_ = type;
    setMinValidSize({ 16, 16 });

    menu_ = new RecordMenu(m_mute_, s_mute_,
                           (type == GIF ? 0x00 : (RecordMenu::MICROPHONE | RecordMenu::SPEAKER)) |
                               RecordMenu::CAMERA | RecordMenu::PAUSE,
                           this);

    prevent_transparent_ = true;

    player_ = new VideoPlayer(this);

    desktop_capturer_ = std::make_unique<DesktopCapturer>();
    if (recording_type_ != GIF) {
        microphone_capturer_ = std::make_unique<AudioCapturer>();
        speaker_capturer_    = std::make_unique<AudioCapturer>();
    }
    encoder_    = std::make_unique<Encoder>();
    dispatcher_ = std::make_unique<Dispatcher>();

    connect(menu_, &RecordMenu::opened, [this](bool) { switchCamera(); });
    connect(menu_, &RecordMenu::stopped, this, &ScreenRecorder::exit);
    connect(menu_, &RecordMenu::stopped, player_, &VideoPlayer::close);
    connect(menu_, &RecordMenu::muted, this, &ScreenRecorder::mute);
    connect(menu_, &RecordMenu::paused, [this]() { dispatcher_->pause(); });
    connect(menu_, &RecordMenu::resumed, [this]() { dispatcher_->resume(); });

    connect(player_, &VideoPlayer::started, [this]() { menu_->camera_checked(true); });
    connect(player_, &VideoPlayer::closed, [this]() { menu_->camera_checked(false); });

    // update time of the menu
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, [this]() { menu_->time(dispatcher_->escaped_ms()); });
}

void ScreenRecorder::switchCamera()
{
    if (player_->ready()) {
        player_->close();
        return;
    }

    if (av::cameras().empty()) {
        LOG(WARNING) << "camera not found";
        return;
    }

    auto camera = Config::instance()["devices"]["cameras"].get<std::string>();
#ifdef _WIN32
    if (!player_->open("video=" + camera, { { "format", "dshow" }, { "filters", "hflip" } })) {
#elif __linux__
    if (!player_->open(camera, { { "format", "v4l2" }, { "filters", "hflip" } })) {
#endif
        player_->close();
    }
}

void ScreenRecorder::record() { status_ == SelectorStatus::INITIAL ? start() : exit(); }

void ScreenRecorder::start()
{
    auto time_str = QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss_zzz").toStdString();
    if (recording_type_ == VIDEO) {
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

    Selector::start();
}

void ScreenRecorder::open_audio_sources()
{
    std::map<std::string, std::string> mic_options{};
    std::map<std::string, std::string> spk_options{};

#ifdef __linux__
    mic_options["format"]        = "pulse";
    mic_options["fragment_size"] = "19200";

    spk_options["format"]        = "pulse";
    spk_options["fragment_size"] = "19200";
#endif

    if (!av::audio_sources().empty()) {
        if (microphone_capturer_ &&
            microphone_capturer_->open(Config::instance()["devices"]["microphones"].get<std::string>(),
                                       mic_options) < 0) {
            LOG(WARNING) << "open microphone failed";
            microphone_capturer_->reset();
            menu_->disable_mic(true);
        }
    }

    if (!av::audio_sinks().empty()) {
        if (speaker_capturer_ &&
            speaker_capturer_->open(Config::instance()["devices"]["speakers"].get<std::string>(),
                                    spk_options) < 0) {
            LOG(WARNING) << "open speaker failed";
            speaker_capturer_->reset();
            menu_->disable_speaker(true);
        }
    }
}

static std::pair<AVPixelFormat, AVHWDeviceType>
set_pix_fmt(const std::unique_ptr<Producer<AVFrame>>& producer,
            const std::unique_ptr<Consumer<AVFrame>>& consumer, const AVCodec *vcodec)
{
    AVHWDeviceType hwaccel = AV_HWDEVICE_TYPE_NONE;
    AVPixelFormat pix_fmt  = AV_PIX_FMT_NONE;

    if (!vcodec) return {};

    const AVCodecHWConfig *config = nullptr;
    for (int i = 0; (config = avcodec_get_hw_config(vcodec, i)); ++i) {
        if (config->pix_fmt != AV_PIX_FMT_NONE) {

            for (const auto& producer_fmt : producer->vformats()) {
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
    for (const auto& fmt : producer->vformats()) {
        if (fmt.hwaccel == AV_HWDEVICE_TYPE_NONE) producer->vfmt.pix_fmt = fmt.pix_fmt;
    }
    //
    return {};
}

void ScreenRecorder::setup()
{
    status_ = SelectorStatus::LOCKED;

    QRect region = selected(scope_ != scope_t::desktop);

    Config::instance()[recording_type_ == VIDEO ? "record" : "gif"]["box"].get<bool>() ? showRegion()
                                                                                       : hide();

    menu_->disable_mic(av::audio_sources().empty());
    menu_->disable_speaker(av::audio_sinks().empty());
    menu_->disable_cam(av::cameras().empty());

    // desktop capturer
    framerate_ = Config::instance()[recording_type_ == VIDEO ? "record" : "gif"]["framerate"].get<int>();
    auto draw_mouse = Config::instance()[recording_type_ == VIDEO ? "record" : "gif"]["mouse"].get<bool>();

    std::string name{};
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
    options["offset_x"]    = std::to_string(region.x());
    options["offset_y"]    = std::to_string(region.y());
    // TODO:
    //   1. rectanle mode: OK, display mode
    //   2. widget   mode: NO, display mode
    //   3. window   mode: NO, display mode
    //   4. display  mode: OK
    //   5. desktop  mode: NO, not supported
    options["show_region"] = "false";
    switch (prey_.type) {
    case hunter::prey_type_t::rectangle: // name = "window=" + std::to_string(window_.handle); break;
    case hunter::prey_type_t::widget:
    case hunter::prey_type_t::window: {
        auto display = probe::graphics::display_contains(selected().center()).value();
        name         = "display=" + std::to_string(display.handle);
        break;
    }
    case hunter::prey_type_t::display: name = "display=" + std::to_string(prey_.handle); break;
    default: LOG(ERROR) << "unsuppored mode"; return;
    }
#endif

    if (desktop_capturer_->open(name, options) < 0) {
        desktop_capturer_->reset();
        exit();
        return;
    }

    if (recording_type_ == VIDEO) {
        open_audio_sources();
    }

    // sources
    dispatcher_->append(desktop_capturer_.get());
    if (microphone_capturer_ && microphone_capturer_->ready()) {
        dispatcher_->append(microphone_capturer_.get());
    }

    if (speaker_capturer_ && speaker_capturer_->ready()) {
        dispatcher_->append(speaker_capturer_.get());
    }

    // outputs
    dispatcher_->set_encoder(encoder_.get());

    // set hwaccel & pixel format if any
    auto [pix_fmt, hwaccel] =
        set_pix_fmt(desktop_capturer_, encoder_, avcodec_find_encoder_by_name(codec_name_.c_str()));

    std::string quality_name = "crf"; // TODO: CRF / CQP
    if (hwaccel != AV_HWDEVICE_TYPE_NONE) {
        quality_name = "cq";
        pix_fmt_     = pix_fmt;
    }

    encoder_options_[quality_name] = video_qualities_[Config::instance()["record"]["quality"]];

    // prepare the filter graph and properties of encoder
    // let dispather decide which pixel format to be used for encoding
    dispatcher_->vfmt.pix_fmt        = pix_fmt_;
    dispatcher_->vfmt.hwaccel        = hwaccel;
    dispatcher_->vfmt.framerate      = { framerate_, 1 };
    dispatcher_->afmt.sample_fmt     = AV_SAMPLE_FMT_FLTP;
    dispatcher_->afmt.channels       = 2;
    dispatcher_->afmt.channel_layout = AV_CH_LAYOUT_STEREO;
    if (dispatcher_->create_filter_graph(filters_, {}) < 0) {
        LOG(INFO) << "create filters failed";
        exit();
        return;
    }

    encoder_options_["vcodec"] = codec_name_;
    encoder_options_["acodec"] = "aac";

    if (encoder_->open(filename_, encoder_options_) < 0) {
        LOG(INFO) << "open encoder failed";
        encoder_->reset();
        exit();
        return;
    }

    // start
    if (dispatcher_->start()) {
        LOG(WARNING) << "RECORDING!! Please exit first.";
        dispatcher_->reset();
        exit();
        return;
    }

    menu_->start();
    timer_->start(50);
}

void ScreenRecorder::exit()
{
    menu_->close();

    dispatcher_->stop();

    if (timer_->isActive()) {
        emit SHOW_MESSAGE(recording_type_ == VIDEO ? "Capturer<VIDEO>" : "Capturer<GIF>",
                          tr("Path: ") + QString::fromStdString(filename_));
        timer_->stop();
    }

    Selector::exit();
}

void ScreenRecorder::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        Selector::exit();
    }

    if (event->key() == Qt::Key_Return) {
        setup();
    }
}
