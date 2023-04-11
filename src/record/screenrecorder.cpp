#include "screenrecorder.h"
#include <QMouseEvent>
#include <QPushButton>
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

    menu_ = new RecordMenu(m_mute_, s_mute_, (type == GIF ? 0x00 : (RecordMenu::MICROPHONE | RecordMenu::SPEAKER)) | RecordMenu::CAMERA | RecordMenu::PAUSE, this);

    prevent_transparent_ = true;

    player_ = new VideoPlayer(this);

    connect(menu_, &RecordMenu::stopped, this, &ScreenRecorder::exit);
    connect(menu_, &RecordMenu::muted, this, &ScreenRecorder::mute);
    connect(menu_, &RecordMenu::opened, [this](bool) { switchCamera(); });
    connect(menu_, &RecordMenu::stopped, player_, &VideoPlayer::close);
    connect(player_, &VideoPlayer::started, [this]() { menu_->camera_checked(true); });
    connect(player_, &VideoPlayer::closed, [this]() { menu_->camera_checked(false); });

    desktop_decoder_ = std::make_unique<Decoder>();
    if (recording_type_ != GIF) {
#ifdef _WIN32
        microphone_decoder_ = std::make_unique<WasapiCapturer>();
        speaker_decoder_ = std::make_unique<WasapiCapturer>();
#else
        microphone_decoder_ = std::make_unique<Decoder>();
        speaker_decoder_ = std::make_unique<Decoder>();
#endif
    }
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

    if (Devices::cameras().empty()) {
        LOG(WARNING) << "camera not found";
        return;
    }

#ifdef _WIN32
    if (!player_->play("video=" + Config::instance()["devices"]["cameras"].get<std::string>(), "dshow", "hflip")) {
#elif __linux__
    if (!player_->play(Devices::cameras()[0].toStdString(), "v4l2", "hflip")) {
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

void ScreenRecorder::open_audio_sources()
{
#ifdef __linux__
    if (!Devices::microphones().empty()) {
        if (microphone_decoder_ && microphone_decoder_->open(Devices::default_audio_source().toStdString(), "pulse", { {"fragment_size", "19200"} }) < 0) {
            LOG(WARNING) << "open microphone failed";
            microphone_decoder_->reset();
            menu_->disable_mic(true);
        }
    }

    if (!Devices::speakers().empty()) {
        if (speaker_decoder_ && speaker_decoder_->open(Devices::default_audio_sink().toStdString(), "pulse", { {"fragment_size", "19200"} }) < 0) {
            LOG(WARNING) << "open speaker failed";
            speaker_decoder_->reset();
            menu_->disable_speaker(true);
        }
    }
#elif _WIN32
    if (!Devices::microphones().empty()) {
        if (microphone_decoder_ && microphone_decoder_->open(DeviceType::DEVICE_MICROPHONE) < 0) {
            LOG(WARNING) << "open microphone failed";
            microphone_decoder_->reset();
            menu_->disable_mic(true);
        }
    }

    if (!Devices::speakers().empty()) {
        if (speaker_decoder_ && speaker_decoder_->open(DeviceType::DEVICE_SPEAKER) < 0) {
            LOG(WARNING) << "open speaker failed";
            speaker_decoder_->reset();
            menu_->disable_speaker(true);
        }
    }
#endif
}

void ScreenRecorder::setup()
{
    status_ = SelectorStatus::LOCKED;
    QRect selected_area = selected();

    Config::instance()[recording_type_ == VIDEO ? "record" : "gif"]["box"].get<bool>() ? showRegion() : hide();

    menu_->disable_mic(Devices::microphones().empty());
    menu_->disable_speaker(Devices::speakers().empty());
    menu_->disable_cam(Devices::cameras().empty());

    auto root_dir = QStandardPaths::writableLocation(recording_type_ == VIDEO ? QStandardPaths::MoviesLocation : QStandardPaths::PicturesLocation).toStdString();
    auto date_time = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz").toStdString();

    filename_ = fmt::format("{}/Capturer_video_{}.{}", root_dir, date_time, (recording_type_ == VIDEO ? "mp4" : "gif"));

    if (desktop_decoder_->open(
#ifdef __linux__
        // https://askubuntu.com/questions/432255/what-is-the-display-environment-variable/432257#432257
        // echo $DISPLAY
        fmt::format("{}.0+{},{}", getenv("DISPLAY"), selected_area.x(), selected_area.y()),
        "x11grab",
        {
            {"framerate", std::to_string(framerate_)},
            {"video_size", fmt::format("{}x{}", (selected_area.width() / 2) * 2, (selected_area.height() / 2) * 2)}
        }
#elif _WIN32
        "desktop",
        "gdigrab",
        {
            {"framerate", std::to_string(framerate_)},
            {"offset_x", std::to_string(selected_area.x())},
            {"offset_y", std::to_string(selected_area.y())},
            {"video_size", fmt::format("{}x{}", (selected_area.width() / 2) * 2, (selected_area.height() / 2) * 2)}
        }
#endif
    ) < 0) {
        desktop_decoder_->reset();
        exit();
        return;
    }
    
    if (recording_type_ == VIDEO) {
        open_audio_sources();
    }

    // sources
    dispatcher_->append(desktop_decoder_.get());
    if (microphone_decoder_ && microphone_decoder_->ready()) {
        dispatcher_->append(microphone_decoder_.get());
    }

    if (speaker_decoder_ && speaker_decoder_->ready()) {
        dispatcher_->append(speaker_decoder_.get());
    }

    // outputs
    dispatcher_->set_encoder(encoder_.get());

    // prepare
    if (dispatcher_->create_filter_graph(filters_, {}) < 0) {
        LOG(INFO) << "create filters failed";
        exit();
        return;
    }

    encoder_->vfmt_.format = pix_fmt_;
    if (encoder_->open(filename_, codec_name_, "aac", true, options_, {}) < 0) {
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
