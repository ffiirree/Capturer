#include "gifcapturer.h"
#include <QFileDialog>
#include <QKeyEvent>
#include <QDebug>
#include <QDateTime>
#include <QStandardPaths>

GifCapturer::GifCapturer(QWidget * parent)
    : Selector(parent)
{
    process_ = new QProcess(this);
}

void GifCapturer::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Escape) {
        status_ = INITIAL;
        this->hide();
    }

    if(event->key() == Qt::Key_Return) {
        status_ = LOCKED;

        this->hide();
        this->setup();
    }
}

void GifCapturer::paintEvent(QPaintEvent *event)
{
    painter_.begin(this);

    QColor bgc = QColor(0, 0, 0, 50);

    if(status_ != NORMAL) {
        auto roi = selected();
        painter_.fillRect(QRect{ 0, 0, width(), roi.y() }, bgc);
        painter_.fillRect(QRect{ 0, roi.y(), roi.x(), roi.height() }, bgc);
        painter_.fillRect(QRect{ roi.x() + roi.width(), roi.y(), width() - roi.x() - roi.width(), roi.height()}, bgc);
        painter_.fillRect(QRect{ 0, roi.y() + roi.height(), width(), height() - roi.y() - roi.height()}, bgc);
    }
    else {
        painter_.fillRect(rect(), bgc);
    }

    painter_.end();

    Selector::paintEvent(event);
}

void GifCapturer::record()
{
    status_ == INITIAL ? start() : end();
}

void GifCapturer::setup()
{
    auto native_movies_path = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    current_time_str_ = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");

    filename_ = native_movies_path + QDir::separator() + "Capturer_gif_" + current_time_str_ + ".gif";

    QStringList args;
    auto selected_area = selected();
    args << "-video_size" << QString::number(selected_area.width()) + "x" + QString::number(selected_area.height())
         << "-framerate" << "25"
         << "-f" << "x11grab"
         << "-i" << ":0.0+" + QString::number(selected_area.x()) + "," + QString::number(selected_area.y())
         << "/tmp/Capturer_gif_" + current_time_str_ + ".mp4";
    process_->start("ffmpeg", args);
}

void GifCapturer::end()
{
    status_ = INITIAL;
    end_ = begin_;

    process_->write("q\n\r");

    process_->waitForFinished();
    QStringList args;
    args << "-y"
         << "-i" << "/tmp/Capturer_gif_" + current_time_str_ + ".mp4"
         << "-vf" << "fps=8,palettegen"
         << "/tmp/Capturer_palette_" + current_time_str_ + ".png";
    process_->start("ffmpeg", args);
    process_->waitForFinished();

    args.clear();
    args << "-i" << "/tmp/Capturer_gif_" + current_time_str_ + ".mp4"
         << "-i" << "/tmp/Capturer_palette_" + current_time_str_ + ".png"
         << "-filter_complex" << "fps=5,paletteuse"
         << filename_;
    process_->start("ffmpeg", args);
}
