#include "screenrecorder.h"
#include <QPixmap>
#include <QMouseEvent>
#include <QProcess>
#include <QPushButton>
#include <QFileDialog>
#include <QDebug>
#include <QStandardPaths>
#include <QDateTime>
#include "detectwidgets.h"

ScreenRecorder::ScreenRecorder(QWidget *parent)
    : Selector(parent)
{
    process_ = new QProcess(this);
}

void ScreenRecorder::record()
{
    status_ == INITIAL ? start() : exit();
}

void ScreenRecorder::setup()
{
    status_ = LOCKED;
    hide();

    auto native_movies_path = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    auto current_date_time = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");

    filename_ = native_movies_path + QDir::separator() + "Capturer_video_" + current_date_time + ".mp4";

    QStringList args;
    auto selected_area = selected();
#ifdef _LINUX
    args << "-video_size" << QString::number(selected_area.width()) + "x" + QString::number(selected_area.height())
         << "-framerate" << QString::number(framerate_)
         << "-f" << "x11grab"
         << "-i" << ":0.0+" + QString::number((selected_area.x())) + "," + QString::number((selected_area.y()))
         << filename_;
#elif _WIN32
    args << "-f" << "gdigrab"
         << "-framerate" << QString::number(framerate_)
         << "-offset_x" << QString::number((selected_area.x())) << "-offset_y" << QString::number((selected_area.y()))
         << "-video_size" << QString::number(selected_area.width()) + "x" + QString::number(selected_area.height())
         << "-i" << "desktop"
         << filename_;
#endif
    process_->start("ffmpeg", args);
}

void ScreenRecorder::exit()
{
    process_->write("q\n\r");

    Selector::exit();
}

void ScreenRecorder::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Escape) {
        exit();
    }

    if(event->key() == Qt::Key_Return) {
        setup();
    }
}

void ScreenRecorder::paintEvent(QPaintEvent *event)
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
