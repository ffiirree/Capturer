#ifndef CAPTURER_VIDEO_PLAYER_H
#define CAPTURER_VIDEO_PLAYER_H

#include <QWidget>
#include <QPainter>
#include <QImage>
#include "mediadecoder.h"
#include "logging.h"

class VideoPlayer : public QWidget {
    Q_OBJECT

public:
	explicit VideoPlayer(QWidget* parent = nullptr)
        : QWidget(nullptr)
	{
        setWindowFlags(Qt::WindowStaysOnTopHint | Qt::Tool | windowFlags());
        setAttribute(Qt::WA_TranslucentBackground);

        thread_ = new QThread(this);
        decoder_ = new MediaDecoder();

        decoder_->moveToThread(thread_);

        connect(thread_, &QThread::started, decoder_, &MediaDecoder::process);
        connect(decoder_, &MediaDecoder::stopped, [this]() { thread_->quit(); });

        connect(decoder_, &MediaDecoder::received, [this]() { QWidget::update(); });
	}

    ~VideoPlayer() { delete decoder_; }

    bool play(std::string name, std::string fmt)
    {
        if (!decoder_->open(name, fmt, "", AV_PIX_FMT_RGB24, {})) {
            return false;
        }

        if (decoder_->width() > 1440 || decoder_->height() > 810) {
            resize(QSize(decoder_->width(), decoder_->height()).scaled(1440, 810, Qt::KeepAspectRatio));
        }
        else {
            resize(decoder_->width(), decoder_->height());
        }

        thread_->start();
        QWidget::show();

        return true;
    }

signals:
    void closed();

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QPainter painter(this);

        QImage image;
        if(decoder_->read(image) >= 0)
            painter.drawImage(rect(), image);
    }

    void closeEvent(QCloseEvent* event) override
    {
        decoder_->stop();
        thread_->quit();
        thread_->wait();

        emit closed();
    }

    MediaDecoder* decoder_{ nullptr };
    QThread* thread_{ nullptr };
};

#endif // !CAPTURER_VIDEO_PLAYER_H