#include "videoplayer.h"

#include "logging.h"

#include <QImage>
#include <QPainter>

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
}

VideoPlayer::VideoPlayer(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::Window | windowFlags());
    setAttribute(Qt::WA_TranslucentBackground);

    CHECK_NOTNULL(frame_ = av_frame_alloc());
    decoder_    = new Decoder();
    dispatcher_ = new Dispatcher();
}

VideoPlayer::~VideoPlayer()
{
    delete decoder_;
    delete dispatcher_;
}

int VideoPlayer::open(const std::string& filename, std::map<std::string, std::string> options)
{
    std::map<std::string, std::string> decoder_options;
    if (options.contains("format")) decoder_options["format"] = options.at("format");

    if (decoder_->open(filename, decoder_options) != 0) {
        decoder_->reset();
        return false;
    }

    dispatcher_->append(decoder_);
    dispatcher_->set_encoder(this);

    dispatcher_->vfmt.pix_fmt = AV_PIX_FMT_RGB24;
    if (dispatcher_->create_filter_graph(options.contains("filters") ? options.at("filters") : "", {})) {
        LOG(INFO) << "create filters failed";
        return false;
    }

    if (vfmt.width > 1'440 || vfmt.height > 810) {
        resize(QSize(vfmt.width, vfmt.height).scaled(1'440, 810, Qt::KeepAspectRatio));
    }
    else {
        resize(vfmt.width, vfmt.height);
    }

    eof_ = 0x00;

    if (dispatcher_->start() < 0) {
        dispatcher_->reset();
        LOG(INFO) << "failed to start";
        return false;
    }

    QWidget::setWindowTitle(QString::fromUtf8(filename.c_str()));

    QWidget::show();

    emit started();
    return true;
}

int VideoPlayer::consume(AVFrame *frame, int type)
{
    if (type != AVMEDIA_TYPE_VIDEO) return -1;

    std::lock_guard lock(mtx_);

    av_frame_unref(frame_);
    av_frame_move_ref(frame_, frame);

    QWidget::update();
    return 0;
}

void VideoPlayer::paintEvent(QPaintEvent *)
{
    if (std::lock_guard lock(mtx_); frame_) {
        QPainter painter(this);

        auto frame = QImage(static_cast<const uchar *>(frame_->data[0]), frame_->width, frame_->height,
                            QImage::Format_RGB888);

        auto winrect = rect();

        auto imgsize = frame.size().scaled(winrect.width(), winrect.height(), Qt::KeepAspectRatio);
        auto imgrect = QRect{ { 0, 0 }, imgsize };

        imgrect.moveCenter(winrect.center());

        painter.drawImage(imgrect, frame);
    }
}

void VideoPlayer::closeEvent(QCloseEvent *)
{
    eof_ = 0x01;

    if (dispatcher_) {
        dispatcher_->stop();
    }
    emit closed();
}