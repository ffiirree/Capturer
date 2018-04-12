#include "gifcapturer.h"
#include <QFileDialog>
#include <QKeyEvent>
#include <QDebug>
#include <QDateTime>

GifCapturer::GifCapturer(QWidget * parent)
    : Selector(parent)
{
    this->setWindowOpacity(0.35);
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
        this->start();
        this->setup();
    }
}

void GifCapturer::paintEvent(QPaintEvent *event)
{
    painter_.begin(this);
    painter_.fillRect(this->rect(),  QColor(0, 0, 0));
    painter_.end();

    Selector::paintEvent(event);
}

void GifCapturer::record()
{
    if(status_ == INITIAL) {
        status_ = NORMAL;
        filename_ = QFileDialog::getSaveFileName(this, tr("Save Gif"), "/", tr("Gif (*.gif)"));

        if(!filename_.isEmpty()) {
            this->show();
            this->update();
        }
    }
    else {
        end();
    }
}

void GifCapturer::setup()
{
    QStringList args;
    auto roi = selected();

    QDateTime current_time = QDateTime::currentDateTime();
    current_time_str_ = current_time.toString("yyyy_MM_dd_hh_mm_ss_zzz");

    args << "-video_size" << (std::to_string(roi.width()) + "x" + std::to_string(roi.height())).c_str()
         << "-framerate" << "25"
         << "-f" << "x11grab"
         << "-i" << (":0.0+" + std::to_string(roi.x()) + "," + std::to_string(roi.y())).c_str()
         << "/tmp/capturer_2_gif_" + current_time_str_ + ".mp4";
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
         << "-i" << "/tmp/capturer_2_gif_" + current_time_str_ + ".mp4"
         << "-vf" << "fps=8,palettegen"
         << "/tmp/capturer_palette_" + current_time_str_ + ".png";
    process_->start("ffmpeg", args);
    process_->waitForFinished();

    args.clear();
    args << "-i" << "/tmp/capturer_2_gif_" + current_time_str_ + ".mp4"
         << "-i" << "/tmp/capturer_palette_" + current_time_str_ + ".png"
         << "-filter_complex" << "fps=5,paletteuse"
         << filename_;
    process_->start("ffmpeg", args);
}
