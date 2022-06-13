#include "screenrecorder.h"
#include <QPixmap>
#include <QMouseEvent>
#include <QPushButton>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDateTime>
#include <fmt/core.h>
#include "widgetsdetector.h"
#include "devices.h"
#include "logging.h"
#include "config.h"
extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
}
#ifdef __linux__
#include <cstdlib>
#endif

ScreenRecorder::ScreenRecorder(int type, QWidget* parent)
    : Selector(parent)
{
    recording_type_ = type;
    setMinValidSize({ 16, 16 });

    menu_ = new RecordMenu(m_mute_, s_mute_, RecordMenu::MICROPHONE | RecordMenu::CAMERA | RecordMenu::PAUSE, this);

    prevent_transparent_ = true;

    player_ = new VideoPlayer(this);

    connect(menu_, &RecordMenu::stopped, this, &ScreenRecorder::exit);
    connect(menu_, &RecordMenu::muted, this, &ScreenRecorder::mute);
    connect(menu_, &RecordMenu::opened, [this](bool opened) { switchCamera(); });
    connect(menu_, &RecordMenu::stopped, player_, &VideoPlayer::close);
    connect(player_, &VideoPlayer::started, [this]() { menu_->camera_checked(true); });
    connect(player_, &VideoPlayer::closed, [this]() { menu_->camera_checked(false); });

    desktop_decoder_ = std::make_unique<Decoder>();
    microphone_decoder_ = std::make_unique<Decoder>();
    encoder_ = std::make_unique<Encoder>();
    dispatcher_ = std::make_unique<Dispatcher>();

    connect(menu_, &RecordMenu::paused, [this]() { dispatcher_->pause(); });
    connect(menu_, &RecordMenu::resumed, [this]() { dispatcher_->resume(); });

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

    if (Devices::cameras().size() == 0) {
        LOG(WARNING) << "camera not found";
        return;
    }

#ifdef _WIN32
    if (!player_->play("video=" + Config::instance()["devices"]["cameras"].get<string>(), "dshow")) {
#elif __linux__
    if (!player_->play("/dev/video0", "v4l2")) {
#endif
        player_->close();
    }
}

void ScreenRecorder::record()
{
    status_ == SelectorStatus::INITIAL ? start() : exit();
}

void ScreenRecorder::start()
{
    if (recording_type_ == VIDEO) {
        pix_fmt_ = AV_PIX_FMT_YUV420P;
        codec_name_ = Config::instance()["record"]["encoder"];
        filters_ = "";
        options_ = { {"crf", video_qualities_[Config::instance()["record"]["quality"]]} };
    }
    else {
        pix_fmt_ = AV_PIX_FMT_PAL8;
        codec_name_ = "gif";
        filters_ = gif_filters_[Config::instance()["gif"]["quality"]];
        options_ = {};
    }

    Selector::start();
}

void ScreenRecorder::setup()
{
    status_ = SelectorStatus::LOCKED;
    QRect selected_area = selected();

    Config::instance()[recording_type_ == VIDEO ? "record" : "gif"]["box"].get<bool>() ? showRegion() : hide();

    menu_->disable_mic(Devices::microphones().size() == 0);
    menu_->disable_cam(Devices::cameras().size() == 0);

    auto root_dir = QStandardPaths::writableLocation(recording_type_ == VIDEO ? QStandardPaths::MoviesLocation : QStandardPaths::PicturesLocation).toStdString();
    auto date_time = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz").toStdString();

    filename_ = fmt::format("{}/Capturer_video_{}.{}", root_dir, date_time, (recording_type_ == VIDEO ? "mp4" : "gif"));
#ifdef __linux__
    if (Devices::microphones().size() > 0 && Devices::microphones().contains("default")) {
        if (microphone_decoder_->open("default", "pulse") < 0) {
            microphone_decoder_->reset();
        }
    }

    if (desktop_decoder_->open(
        // https://askubuntu.com/questions/432255/what-is-the-display-environment-variable/432257#432257
        // echo $DISPLAY
        fmt::format("{}.0+{},{}", getenv("DISPLAY"), selected_area.x(), selected_area.y()),
        "x11grab",
        {
            {"framerate", std::to_string(framerate_)},
            {"video_size", fmt::format("{}x{}", (selected_area.width() / 2) * 2, (selected_area.height() / 2) * 2)}
        }
    ) < 0) {
        desktop_decoder_->reset();
        exit();
        return;
    }
#elif _WIN32
    if (desktop_decoder_->open(
        "desktop",
        "gdigrab",
        {
            {"framerate", std::to_string(framerate_)},
            {"offset_x", std::to_string(selected_area.x())},
            {"offset_y", std::to_string(selected_area.y())},
            {"video_size", fmt::format("{}x{}", (selected_area.width() / 2) * 2, (selected_area.height() / 2) * 2)}
        }
    ) < 0) {
        desktop_decoder_->reset();
        exit();
        return;
    }

    if (Devices::microphones().size() > 0) {
        if (microphone_decoder_->open(
            fmt::format("audio={}", Config::instance()["devices"]["microphones"].get<std::string>()),
            "dshow"
        ) < 0) {
            microphone_decoder_->reset();
        }
    }
#endif

    dispatcher_->append(desktop_decoder_.get());
    if (Devices::microphones().size() > 0 && microphone_decoder_->ready()) {
        dispatcher_->append(microphone_decoder_.get());
    }

    encoder_->format(pix_fmt_);
    encoder_->enable(AVMEDIA_TYPE_AUDIO, Devices::microphones().size() > 0);
    auto& coder = dispatcher_->append(encoder_.get());

    if (dispatcher_->create_filter_graph(filters_) < 0) {
        LOG(INFO) << "create filters failed";
        exit();
        return;
    }
    
    encoder_->width(av_buffersink_get_w(coder.video_sink_ctx));
    encoder_->height(av_buffersink_get_h(coder.video_sink_ctx));
    encoder_->framerate(av_buffersink_get_frame_rate(coder.video_sink_ctx));
    encoder_->sample_aspect_ratio(av_buffersink_get_sample_aspect_ratio(coder.video_sink_ctx));
    encoder_->v_stream_tb(av_buffersink_get_time_base(coder.video_sink_ctx));
    if (coder.audio_sink_ctx) {
        encoder_->channels(av_buffersink_get_channels(coder.audio_sink_ctx));
        encoder_->channel_layout(av_buffersink_get_channel_layout(coder.audio_sink_ctx));
        encoder_->sample_rate(av_buffersink_get_sample_rate(coder.audio_sink_ctx));
        encoder_->a_stream_tb(av_buffersink_get_time_base(coder.audio_sink_ctx));
    }

    if (encoder_->open(filename_, codec_name_, true, options_) < 0) {
        LOG(INFO) << "open encoder failed";
        encoder_->reset();
        exit();
        return;
    }

    if (dispatcher_->start()) {
        LOG(WARNING) << "RECARDING!! Exit first.";
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
        emit SHOW_MESSAGE(recording_type_ == VIDEO ? "Capturer<VIDEO>" : "Capturer<GIF>", tr("Path: ") + QString::fromStdString(filename_));
        timer_->stop();
    }

    Selector::exit();
}

void ScreenRecorder::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Escape) {
        Selector::exit();
    }

    if(event->key() == Qt::Key_Return) {
        setup();
    }
}
