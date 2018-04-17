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
    status_ == INITIAL ? start() : end();
}

void ScreenRecorder::setup()
{
    auto native_movies_path = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    auto current_date_time = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");

    filename_ = native_movies_path + QDir::separator() + "Capturer_video_" + current_date_time + ".mp4";

    QStringList args;
    auto selected_area = selected();
    args << "-video_size" << QString::number(selected_area.width()) + "x" + QString::number(selected_area.height())
         << "-framerate" << "25"
         << "-f" << "x11grab"
         << "-i" << ":0.0+" + QString::number((selected_area.x())) + "," + QString::number((selected_area.y()))
         << filename_;
    process_->start("ffmpeg", args);
}

void ScreenRecorder::end()
{
    status_ = INITIAL;
    end_ = begin_;

    process_->write("q\n\r");
}

void ScreenRecorder::keyPressEvent(QKeyEvent *event)
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

void ScreenRecorder::paintEvent(QPaintEvent *event)
{
    painter_.begin(this);

    QColor bgc = QColor(0, 0, 0, 50);

    auto roi = (status_ == NORMAL ? DetectWidgets::window() : selected());
    painter_.fillRect(QRect{ 0, 0, width(), roi.y() }, bgc);
    painter_.fillRect(QRect{ 0, roi.y(), roi.x(), roi.height() }, bgc);
    painter_.fillRect(QRect{ roi.x() + roi.width(), roi.y(), width() - roi.x() - roi.width(), roi.height()}, bgc);
    painter_.fillRect(QRect{ 0, roi.y() + roi.height(), width(), height() - roi.y() - roi.height()}, bgc);

    painter_.fillRect(selected(), QColor(0, 0, 0, 1)); // Make windows happy.

    painter_.end();

    Selector::paintEvent(event);
}
