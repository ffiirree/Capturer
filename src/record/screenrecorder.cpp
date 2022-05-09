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

ScreenRecorder::ScreenRecorder(int type, QWidget *parent)
    : Selector(parent)
{
    recording_type_ = type;

    menu_ = new RecordMenu(m_mute_, s_mute_, RecordMenu::CAMERA | RecordMenu::PAUSE);
    prevent_transparent_ = true;

    player_ = new VideoPlayer(this);

    connect(menu_, &RecordMenu::stopped, this, &ScreenRecorder::exit);
    connect(menu_, &RecordMenu::muted, this, &ScreenRecorder::mute);
    connect(menu_, &RecordMenu::opened, [this](bool opened) {
        if (Devices::cameras().size() == 0) {
            LOG(WARNING) << "camera not found";
            return;
        }
        QString camera_name = Config::instance()["devices"]["cameras"].get<QString>();
        LOG(INFO) << "camera name : " << camera_name;
        opened ? player_->play("video=" + camera_name.toStdString(), "dshow") : player_->close();
    });
    connect(menu_, &RecordMenu::stopped, player_, &VideoPlayer::close);
    connect(player_, &VideoPlayer::closed, [this]() { menu_->close_camera(); });

    decoder_ = new MediaDecoder();
    encoder_ = new MediaEncoder(decoder_);

    decoder_thread_ = new QThread(this);
    encoder_thread_ = new QThread(this);

    decoder_->moveToThread(decoder_thread_);
    encoder_->moveToThread(encoder_thread_);

    connect(decoder_thread_, &QThread::started, decoder_, &MediaDecoder::process);
    connect(encoder_thread_, &QThread::started, encoder_, &MediaEncoder::process);
    connect(encoder_thread_, &QThread::finished, menu_, &RecordMenu::close);
    
    // start encoder after decoder is started
    connect(decoder_, &MediaDecoder::started, [this]() { encoder_thread_->start(); });
    connect(decoder_, &MediaDecoder::stopped, [this]() { decoder_thread_->quit(); });

    // close decoder 
    connect(encoder_, &MediaEncoder::stopped, [this]() { decoder_->stop(); encoder_thread_->quit(); });

    connect(menu_, &RecordMenu::stopped, [this]() { decoder_->stop(); });
    connect(menu_, &RecordMenu::paused, [this]() { decoder_->pause(); });
    connect(menu_, &RecordMenu::resumed, [this]() { decoder_->resume(); });

    // update time of the menu
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, [this]() { menu_->time(encoder_->escaped_ms()); });
}

void ScreenRecorder::record()
{
    status_ == SelectorStatus::INITIAL ? start() : exit();
}

void ScreenRecorder::start()
{
    if (decoder_->running() || decoder_->opened() || encoder_->opened()) {
        LOG(WARNING) << "RECARDING!! Exit first.";
        return;
    }

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
    Config::instance()[recording_type_ == VIDEO ? "record" : "gif"]["box"].get<bool>() ? hideMask(true) : hide();

    auto root_dir = QStandardPaths::writableLocation(recording_type_ == VIDEO ? QStandardPaths::MoviesLocation : QStandardPaths::PicturesLocation).toStdString();
    auto date_time = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz").toStdString();

    filename_ = fmt::format("{}/Capturer_video_{}.{}", root_dir, date_time, (recording_type_ == VIDEO ? "mp4" : "gif"));

    auto selected_area = selected();
#ifdef __linux__
    decoder_->open(
        QString(":0.0+%1,%2").arg((selected_area.x() / 2) * 2).arg((selected_area.y()) / 2 * 2).toStdString(),
        "x11grab",
        filters_,
        pix_fmt_,
        {
            {"framerate", std::to_string(framerate_)},
            {"video_size", fmt::format("{}x{}", (selected_area.width() / 2) * 2, (selected_area.height() / 2) * 2)}
        }
    );
#elif _WIN32
    decoder_->open(
        "desktop",
        "gdigrab",
        filters_,
        pix_fmt_,
        {
            {"framerate", std::to_string(framerate_)},
            {"offset_x", std::to_string(selected_area.x())},
            {"offset_y", std::to_string(selected_area.y())},
            {"video_size", fmt::format("{}x{}", (selected_area.width() / 2) * 2, (selected_area.height() / 2) * 2)}
        }
    );
#endif
    // start recording
    if (decoder_->opened()) {
        // open the output file
        encoder_->open(
            filename_,
            codec_name_,
            pix_fmt_,
            { framerate_, 1 },
            recording_type_ != VIDEO,
            options_
        );

        if (encoder_->opened()) {
            decoder_thread_->start();

            menu_->start();
            timer_->start(50);
        }
    }
}

void ScreenRecorder::exit()
{
    decoder_->stop();
    menu_->close();
    decoder_thread_->wait();
    encoder_thread_->wait();

    if (timer_->isActive()) {
        emit SHOW_MESSAGE(recording_type_ == VIDEO ? "Capturer<VIDEO>" : "Capturer<GIF>", tr("Path: ") + QString::fromStdString(filename_));
        timer_->stop();
    }

    hideMask(false);
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
