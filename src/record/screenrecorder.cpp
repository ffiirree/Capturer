#include "screenrecorder.h"
#include <QPixmap>
#include <QMouseEvent>
#include <QPushButton>
#include <QFileDialog>
#include <QStandardPaths>
#include "widgetsdetector.h"
#include "devices.h"
#include "logging.h"

ScreenRecorder::ScreenRecorder(int type, QWidget *parent)
    : Selector(parent)
{
    type_ = type;
    pix_fmt_ = (type_ == VIDEO) ? AV_PIX_FMT_YUV420P : AV_PIX_FMT_PAL8;

    menu_ = new RecordMenu(m_mute_, s_mute_, RecordMenu::RECORD_MENU_NONE);
    prevent_transparent_ = true;

    player_ = new VideoPlayer(this);

    connect(menu_, &RecordMenu::stopped, this, &ScreenRecorder::exit);
    connect(menu_, &RecordMenu::muted, this, &ScreenRecorder::mute);
    connect(menu_, &RecordMenu::opened, [this](bool opened) {
        if (Config::instance()["devices"]["cameras"].is_null()) {
            LOG(WARNING) << "not found ";
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
    connect(decoder_, &MediaDecoder::started, [this]() { encoder_thread_->start(); });
    connect(encoder_thread_, &QThread::started, encoder_, &MediaEncoder::process);
    
    connect(decoder_, &MediaDecoder::stopped, [this]() { decoder_thread_->quit(); });
    connect(encoder_, &MediaEncoder::stopped, [this]() { encoder_thread_->quit(); });

    connect(menu_, &RecordMenu::stopped, decoder_, &MediaDecoder::stop);
}

void ScreenRecorder::record()
{
    status_ == SelectorStatus::INITIAL ? start() : exit();
}

void ScreenRecorder::start()
{
    Selector::start();
}

void ScreenRecorder::setup()
{
    if (decoder_->running() || decoder_->opened() || encoder_->opened()) {
        LOG(WARNING) << "RECARDING!! Exit first.";
        return;
    }

    menu_->start();

    status_ = SelectorStatus::LOCKED;
    hide();

    auto native_movies_path = QStandardPaths::writableLocation(type_ == VIDEO ? QStandardPaths::MoviesLocation : QStandardPaths::PicturesLocation);
    auto current_date_time = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");

    filename_ = native_movies_path + QDir::separator() + "Capturer_video_" + current_date_time + (type_ == VIDEO ? ".mp4" : ".gif");

    auto selected_area = selected();
#ifdef __linux__
    decoder_->open(
        QString(":0.0+%1,%2").arg((selected_area.x() / 2) * 2).arg((selected_area.y()) / 2 * 2).toStdString(),
        "x11grab",
        type_ == VIDEO ? "" : "[0:v] split [a][b];[a] palettegen=stats_mode=single [p];[b][p] paletteuse=dither=none:new=1",
        type_ == VIDEO ? AV_PIX_FMT_YUV420P : AV_PIX_FMT_PAL8,
        {
            {"framerate", std::to_string(framerate_)},
            {"video_size", QString("%1x%2").arg((selected_area.width() / 2) * 2).arg((selected_area.height() / 2) * 2).toStdString()}
        }
    );
#elif _WIN32
    decoder_->open(
        "desktop",
        "gdigrab",
        type_ == VIDEO ? "" : "[0:v] split [a][b];[a] palettegen=stats_mode=single [p];[b][p] paletteuse=dither=none:new=1",
        pix_fmt_,
        {
            //{"framerate", std::to_string(framerate_)},
            {"offset_x", std::to_string(selected_area.x())},
            {"offset_y", std::to_string(selected_area.y())},
            {"video_size", QString("%1x%2").arg((selected_area.width() / 2) * 2).arg((selected_area.height() / 2) * 2).toStdString()}
        }
    );
#endif
    // TODO: [GIF] ffmpeg -i <in> -filter_complex "[0:v] fps=15,scale=640:-1:flags=lanczos,split [a][b];[a] palettegen=stats_mode=diff [p];[b][p] paletteuse=dither=floyd_steinberg" out.gif
    encoder_->open(filename_.toStdString(), type_ == VIDEO ? "libx264" : "gif", pix_fmt_, { framerate_, 1 }, type_ != VIDEO);

    if (decoder_->opened() && encoder_->opened())
    {
        decoder_thread_->start();
    }
}

void ScreenRecorder::exit()
{
    decoder_->stop();
    menu_->close();
    emit SHOW_MESSAGE(type_ == VIDEO ? "Capturer<VIDEO>" : "Capturer<GIF>", tr("Path: ") + filename_);
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
