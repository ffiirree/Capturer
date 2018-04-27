#include "gifcapturer.h"
#include <QFileDialog>
#include <QKeyEvent>
#include <QDebug>
#include <QDateTime>
#include <QStandardPaths>
#include "detectwidgets.h"

GifCapturer::GifCapturer(QWidget * parent)
    : Selector(parent)
{
    process_ = new QProcess(this);
}

void GifCapturer::record()
{
    status_ == INITIAL ? start() : exit();
}

void GifCapturer::setup()
{
    status_ = LOCKED;
    hide();

    auto native_pictures_path = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    current_time_str_ = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");

    filename_ = native_pictures_path + QDir::separator() + "Capturer_gif_" + current_time_str_ + ".gif";

    QStringList args;
    auto selected_area = selected();
#ifdef _LINUX
    args << "-video_size" << QString::number(selected_area.width()) + "x" + QString::number(selected_area.height())
         << "-framerate" << QString::number(framerate_)
         << "-f" << "x11grab"
         << "-i" << ":0.0+" + QString::number(selected_area.x()) + "," + QString::number(selected_area.y())
         << "/tmp/Capturer_gif_" + current_time_str_ + ".mp4";
#elif _WIN32
    auto temp_dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    temp_video_path_ = temp_dir + QDir::separator() + "Capturer_gif_" + current_time_str_ + ".mp4";
    temp_palette_path_ = temp_dir + QDir::separator() + "Capturer_palette_" + current_time_str_ + ".png";

    args << "-f" << "gdigrab"
         << "-framerate" << QString::number(framerate_)
         << "-offset_x" << QString::number((selected_area.x())) << "-offset_y" << QString::number((selected_area.y()))
         << "-video_size" << QString::number(selected_area.width()) + "x" + QString::number(selected_area.height())
         << "-i" << "desktop"
         << temp_video_path_;
#endif
    process_->start("ffmpeg", args);
}

void GifCapturer::exit()
{
    process_->write("q\n\r");

    process_->waitForFinished();
    QStringList args;
#ifdef _LINUX
    args << "-y"
         << "-i" << "/tmp/Capturer_gif_" + current_time_str_ + ".mp4"
         << "-vf" << "fps=" << QString::number(fps_) << ",palettegen"
         << "/tmp/Capturer_palette_" + current_time_str_ + ".png";
    process_->start("ffmpeg", args);
    process_->waitForFinished();

    args.clear();
    args << "-i" << "/tmp/Capturer_gif_" + current_time_str_ + ".mp4"
         << "-i" << "/tmp/Capturer_palette_" + current_time_str_ + ".png"
         << "-filter_complex" << "fps=8,paletteuse"
         << filename_;
#elif _WIN32
    args << "-y"
         << "-i" << temp_video_path_
         << "-vf" << "fps=" + QString::number(fps_) + ",palettegen"
         << temp_palette_path_;
    process_->start("ffmpeg", args);
    process_->waitForFinished();

    args.clear();
    args << "-i" << temp_video_path_
         << "-i" << temp_palette_path_
         << "-filter_complex" << "fps=" + QString::number(fps_) + ",paletteuse"
         << filename_;
#endif
    process_->start("ffmpeg", args);

    Selector::exit();
}


void GifCapturer::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Escape) {
        exit();
    }

    if(event->key() == Qt::Key_Return) {
        setup();
    }
}

void GifCapturer::paintEvent(QPaintEvent *event)
{
    painter_.begin(this);

    auto roi = (status_ == NORMAL ? DetectWidgets::window() : selected());
    painter_.fillRect(rect(), QColor(0, 0, 0, 1)); // Make windows happy.

    painter_.fillRect(QRect{ 0, 0, width(), roi.y() }, mask_color_);
    painter_.fillRect(QRect{ 0, roi.y(), roi.x(), roi.height() }, mask_color_);
    painter_.fillRect(QRect{ roi.x() + roi.width(), roi.y(), width() - roi.x() - roi.width(), roi.height()}, mask_color_);
    painter_.fillRect(QRect{ 0, roi.y() + roi.height(), width(), height() - roi.y() - roi.height()}, mask_color_);

    painter_.end();

    Selector::paintEvent(event);
}
