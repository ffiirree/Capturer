#include "videoplayer.h"
extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
}

bool VideoPlayer::play(const std::string& name, const std::string& fmt, const std::string& filters)
{
    if (decoder_->open(name, fmt, {}) != 0) {
        return false;
    }

    dispatcher_->append(decoder_);
    auto& coder = dispatcher_->append(this);

    if (dispatcher_->create_filter_graph(filters)) {
        LOG(INFO) << "create filters failed";
        return false;
    }

    int width = av_buffersink_get_w(coder.video_sink_ctx);
    int height = av_buffersink_get_h(coder.video_sink_ctx);

    if (width > 1440 || height > 810) {
        resize(QSize(width, height).scaled(1440, 810, Qt::KeepAspectRatio));
    }
    else {
        resize(width, height);
    }

    if (dispatcher_->start() < 0) {
        LOG(INFO) << "failed to start";
        return false;
    }

    QWidget::show();

    return true;
}