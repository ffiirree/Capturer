#include "screenrecorder.h"
#include <QPixmap>
#include <QMouseEvent>
#include <QProcess>
#include <QPushButton>
#include <QFileDialog>
#include <QDebug>

ScreenRecorder::ScreenRecorder(QWidget *parent)
    : Selector(parent)
{
    this->setWindowOpacity(0.35);

    process_ = new QProcess(this);
}

void ScreenRecorder::record()
{
    if(status_ == INITIAL) {
        status_ = NORMAL;
        filename_ = QFileDialog::getSaveFileName(this, tr("Save Video"), "/", tr("Video Files (*.mp4)"));

        if(!filename_.isEmpty()) {
            this->show();
            this->update();
        }
    }
    else {
        end();
    }
}

void ScreenRecorder::setup()
{
    QStringList args;
    auto roi = selected();

    args << "-video_size" << (std::to_string(roi.width()) + "x" + std::to_string(roi.height())).c_str()
         << "-framerate" << "25"
         << "-f" << "x11grab"
         << "-i" << (":0.0+" + std::to_string(roi.x()) + "," + std::to_string(roi.y())).c_str()
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
        this->start();
        this->setup();
    }
}

void ScreenRecorder::paintEvent(QPaintEvent *event)
{
    painter_.begin(this);
    painter_.fillRect(this->rect(),  QColor(0, 0, 0));
    painter_.end();

    Selector::paintEvent(event);
}
