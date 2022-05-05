#include "gifcapturer.h"
#include <QFileDialog>
#include <QKeyEvent>
#include <QStandardPaths>
#include "widgetsdetector.h"

GifCapturer::GifCapturer(QWidget * parent)
    : Selector(parent)
{
    record_menu_ = new RecordMenu(0, 0, RecordMenu::RECORD_MENU_NONE);
    prevent_transparent_ = true;

    connect(record_menu_, &RecordMenu::stopped, this, &GifCapturer::exit);

    player_ = new VideoPlayer(this);

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

    connect(record_menu_, &RecordMenu::stopped, decoder_, &MediaDecoder::stop);
}

void GifCapturer::record()
{
    status_ == SelectorStatus::INITIAL ? start() : exit();
}

void GifCapturer::setup()
{
    record_menu_->start();

    status_ = SelectorStatus::LOCKED;
    hide();

    auto native_pictures_path = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    current_time_str_ = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");

    filename_ = native_pictures_path + QDir::separator() + "Capturer_gif_" + current_time_str_ + ".gif";

    auto selected_area = selected();
#ifdef __linux__
    decoder_->open(
        QString(":0.0+%1,%2").arg((selected_area.x() / 2) * 2).arg((selected_area.y()) / 2 * 2).toStdString(),
        "x11grab",
        AV_PIX_FMT_YUV420P,
        {
            {"framerate", std::to_string(framerate_)},
            {"video_size", QString("%1x%2").arg((selected_area.width() / 2) * 2).arg((selected_area.height() / 2) * 2).toStdString()}
        }
    );
#elif _WIN32
    decoder_->open(
        "desktop",
        "gdigrab",
        "",
        AV_PIX_FMT_PAL8,
        {
            {"framerate", std::to_string(framerate_)},
            {"offset_x", std::to_string(selected_area.x())},
            {"offset_y", std::to_string(selected_area.y())},
            {"video_size", QString("%1x%2").arg((selected_area.width() / 2) * 2).arg((selected_area.height() / 2) * 2).toStdString()}
        }
    );
#endif
    // TODO: ffmpeg -i .\in.mp4 -filter_complex "[0:v] fps=15,scale=640:-1,split [a][b];[a] palettegen [p];[b][p] paletteuse" out.gif
    encoder_->open(filename_.toStdString(), "gif", AV_PIX_FMT_PAL8, { framerate_, 1 }, true, { });

    if (decoder_->opened() && encoder_->opened())
    {
        decoder_thread_->start();
    }
}

void GifCapturer::exit()
{
    decoder_->stop();

    record_menu_->close();
    emit SHOW_MESSAGE("Capturer<GIF>", tr("Path: ") + filename_);

    Selector::exit();
}


void GifCapturer::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Escape) {
        Selector::exit();
    }

    if(event->key() == Qt::Key_Return) {
        setup();
    }
}

