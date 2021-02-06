#include "screenrecorder.h"
#include <QPixmap>
#include <QMouseEvent>
#include <QProcess>
#include <QPushButton>
#include <QFileDialog>
#include <QStandardPaths>
#include "detectwidgets.h"
#include "devices.h"
#include "logging.h"

ScreenRecorder::ScreenRecorder(QWidget *parent)
    : Selector(parent)
{
    process_ = new QProcess(this);
    menu_ = new RecordMenu();
    prevent_transparent_ = true;

    connect(menu_, &RecordMenu::STOP, this, &ScreenRecorder::exit);
//    connect(menu_, &RecordMenu::mute, this, &ScreenRecorder::mute);
}

void ScreenRecorder::record()
{
    status_ == INITIAL ? start() : exit();
}

void ScreenRecorder::start()
{
    Devices::refresh();
    Selector::start();
}

void ScreenRecorder::setup()
{
    menu_->start();

    status_ = LOCKED;
    hide();

    auto native_movies_path = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    auto current_date_time = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");

    filename_ = native_movies_path + QDir::separator() + "Capturer_video_" + current_date_time + ".mp4";

    QStringList args;
    auto selected_area = selected();
#ifdef __linux__
    args << "-f"            << "x11grab"
         << "-framerate"    << QString("%1").arg(framerate_)
         << "-video_size"   << QString("%1x%2").arg(selected_area.width()).arg(selected_area.height())
         << "-i"            << QString(":0.0+%1,%2").arg(selected_area.x()).arg(selected_area.y())
         << "-pix_fmt"      << "yuv420p"
         << "-vf"           << "scale=trunc(iw/2)*2:trunc(ih/2)*2"
         << filename_;
#elif _WIN32
    auto audio_devices = Devices::audioDevices();
    if(!audio_devices.empty() && !mute_) {
        args << "-f" << "dshow"
             << "-i" << "audio=" + audio_devices.at(0).first;
    }
    args << "-f"            << "gdigrab"
         << "-framerate"    << QString("%1").arg(framerate_)
         << "-offset_x"     << QString("%1").arg(selected_area.x())
         << "-offset_y"     << QString("%1").arg(selected_area.y())
         << "-video_size"   << QString("%1x%2").arg(selected_area.width()).arg(selected_area.height())
         << "-i"            << "desktop"
         << "-pix_fmt"      << "yuv420p"
         << "-vf"           << "scale=trunc(iw/2)*2:trunc(ih/2)*2"
         << filename_;

    LOG(INFO) << args;
#endif
    process_->start("ffmpeg", args);
}

void ScreenRecorder::exit()
{
    process_->write("q\n\r");
    menu_->stop();
    emit SHOW_MESSAGE("Capturer<VIDEO>", tr("Path: ") + filename_);
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
