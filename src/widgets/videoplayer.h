#ifndef CAPTURER_VIDEO_PLAYER_H
#define CAPTURER_VIDEO_PLAYER_H

#include <QWidget>
#include <QPainter>
#include <QImage>
#include "decoder.h"
#include "logging.h"
#include "consumer.h"
#include "dispatcher.h"

class VideoPlayer : public QWidget, public Consumer<AVFrame> {
    Q_OBJECT

public:
	explicit VideoPlayer(QWidget* parent = nullptr)
        : QWidget(nullptr)
	{
        setWindowFlags(Qt::WindowStaysOnTopHint | Qt::Tool | windowFlags());
        setAttribute(Qt::WA_TranslucentBackground);

        CHECK_NOTNULL(frame_ = av_frame_alloc());
        decoder_ = new Decoder();
        dispatcher_ = new Dispatcher();
	}

    ~VideoPlayer() override
    {
        delete decoder_;
        delete dispatcher_;
    }

    int run() override { return 0; };
    bool ready() const override { return true; }
    int consume(AVFrame* frame, int type) override
    {
        if (type != AVMEDIA_TYPE_VIDEO) return -1;

        std::lock_guard lock(mtx_);

        av_frame_unref(frame_);
        av_frame_move_ref(frame_, frame);

        QWidget::update();
        return 0;
    };
    bool full(int) override { return false; }
    int format() const override { return AV_PIX_FMT_RGB24; }

    bool play(const std::string& name, const std::string& fmt = "", const std::string& filters = "")
    {
        if (decoder_->open(name, fmt, {}) != 0) {
            return false;
        }

        CHECK(dispatcher_->append(decoder_) == 0);
        CHECK_NOTNULL(sink_ = dispatcher_->append(this));

        if (dispatcher_->create_filter_graph(filters)) {
            LOG(INFO) << "create filters failed";
            return false;
        }

        int width = av_buffersink_get_w(sink_);
        int height = av_buffersink_get_h(sink_);

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

signals:
    void closed();

protected:
    void paintEvent(QPaintEvent* event) override
    {
        std::lock_guard lock(mtx_);

        if (frame_) {
            QPainter painter(this);
            painter.drawImage(rect(), QImage(static_cast<const uchar*>(frame_->data[0]), frame_->width, frame_->height, QImage::Format_RGB888));
        }
    }

    void closeEvent(QCloseEvent* event) override
    {
        if (dispatcher_) {
            dispatcher_->stop();
        }
        emit closed();
    }

    Decoder* decoder_{ nullptr };
    Dispatcher* dispatcher_{ nullptr };

    std::mutex mtx_;
    AVFrame* frame_{ nullptr };
    AVFilterContext* sink_{ nullptr }; // dispatcher holds the ownership
};

#endif // !CAPTURER_VIDEO_PLAYER_H