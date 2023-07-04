#include "videoplayer.h"

#include "logging.h"

#include <QHBoxLayout>

VideoPlayer::VideoPlayer(QWidget *parent)
    : FramelessWindow(parent)
{
    setWindowFlags(windowFlags() | Qt::Window | Qt::WindowStaysOnTopHint);

    decoder_    = new Decoder();
    dispatcher_ = new Dispatcher();

    texture_ = new TextureWidget(this);
    setLayout(new QHBoxLayout());
    layout()->setSpacing(0);
    layout()->setContentsMargins({});
    layout()->addWidget(texture_);
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

    texture_->present(frame);
    return 0;
}

void VideoPlayer::closeEvent(QCloseEvent *)
{
    eof_ = 0x01;

    if (dispatcher_) {
        dispatcher_->stop();
    }
    emit closed();
}