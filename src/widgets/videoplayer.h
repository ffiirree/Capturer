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
	}

    void play(std::string name, std::string fmt)
    {
        if (!camera_.open(name, fmt, "", AV_PIX_FMT_RGB24, {})) {
            return;
        }

        resize(camera_.width(), camera_.height());

        QThread* thread = new QThread();

        camera_.moveToThread(thread);
        connect(thread, &QThread::started, &camera_, &MediaDecoder::process);
        connect(&camera_, &MediaDecoder::received, [this]() { QWidget::update(); });
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

        QImage image;
        if(camera_.read(image) >= 0)
            painter.drawImage(rect(), image);
    }
    void closeEvent(QCloseEvent* event) override
    {
        camera_.stop();
        emit closed();
    }

	MediaDecoder camera_;
};

#endif // !CAPTURER_VIDEO_PLAYER_H