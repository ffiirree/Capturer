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
    void reset() override { }

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
    bool accepts(int type) const override { return type == AVMEDIA_TYPE_VIDEO; }
    int format(int) const override { return AV_PIX_FMT_RGB24; }

    bool play(const std::string& name, const std::string& fmt = "", const std::string& filters = "");

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
};

#endif // !CAPTURER_VIDEO_PLAYER_H