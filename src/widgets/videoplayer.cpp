#include "videoplayer.h"
#include <QPainter>
#include <QImage>
extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
}
#include "logging.h"

VideoPlayer::VideoPlayer(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::Tool | windowFlags());
    setAttribute(Qt::WA_TranslucentBackground);

    CHECK_NOTNULL(frame_ = av_frame_alloc());
    decoder_ = new Decoder();
    dispatcher_ = new Dispatcher();
}

VideoPlayer::~VideoPlayer()
{
    delete decoder_;
    delete dispatcher_;
}

bool VideoPlayer::play(const std::string& name, const std::string& fmt, const std::string& filters)
{
    if (decoder_->open(name, fmt, {}) != 0) {
        decoder_->reset();
        return false;
    }

    dispatcher_->append(decoder_);
    dispatcher_->set_encoder(this);

    if (dispatcher_->create_filter_graph(filters, {})) {
        LOG(INFO) << "create filters failed";
        return false;
    }

    auto sink = dispatcher_->find_sink(this, AVMEDIA_TYPE_VIDEO);
    if (sink == nullptr) {
        LOG(INFO) << "not found the sink filter";
        return false;
    }

    int width = av_buffersink_get_w(sink);
    int height = av_buffersink_get_h(sink);

    if (width > 1440 || height > 810) {
        resize(QSize(width, height).scaled(1440, 810, Qt::KeepAspectRatio));
    }
    else {
        resize(width, height);
    }

    eof_ = 0x00;

    if (dispatcher_->start() < 0) {
        dispatcher_->reset();
        LOG(INFO) << "failed to start";
        return false;
    }

    QWidget::show();

    emit started();
    return true;
}

int VideoPlayer::consume(AVFrame* frame, int type)
{
    if (type != AVMEDIA_TYPE_VIDEO) return -1;

    std::lock_guard lock(mtx_);

    av_frame_unref(frame_);
    av_frame_move_ref(frame_, frame);

    QWidget::update();
    return 0;
}

void VideoPlayer::paintEvent(QPaintEvent* event)
{
    if (std::lock_guard lock(mtx_); frame_) {
        QPainter painter(this);
        painter.drawImage(
            rect(), 
            QImage(static_cast<const uchar*>(frame_->data[0]), frame_->width, frame_->height, QImage::Format_RGB888)
        );
    }
}

void VideoPlayer::closeEvent(QCloseEvent* event)
{
    eof_ = 0x01;

    if (dispatcher_) {
        dispatcher_->stop();
    }
    emit closed();
}