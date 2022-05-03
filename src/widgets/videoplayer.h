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
	}

    void play()
    {
        if (!camera_.open("video=HD WebCam", "dshow", {})) {
            return;
        }

        resize(camera_.width(), camera_.height());

        QThread* thread = new QThread();

        camera_.moveToThread(thread);
        connect(thread, &QThread::started, &camera_, &MediaDecoder::process);
        connect(&camera_, &MediaDecoder::received, this, [=]() { 
            LOG(INFO) << "new frame";

            QWidget::update(); 
        });
        thread->start();
        QWidget::show();
    }

signals:
    void closed();

public slots:
    void close()
    {
        QWidget::close();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QPainter painter(this);
        auto frame = camera_.frame();
        painter.drawImage(rect(), frame);
    }
    void closeEvent(QCloseEvent* event) override
    {
        camera_.stop();
        emit closed();
    }

	MediaDecoder camera_;
};

#endif // !CAPTURER_VIDEO_PLAYER_H