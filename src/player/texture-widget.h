#ifndef CAPTURER_TEXTURE_WIDGET_H
#define CAPTURER_TEXTURE_WIDGET_H

#include "libcap/ffmpeg-wrapper.h"

#include <mutex>
#include <QWidget>

class TextureWidget final : public QWidget
{
    Q_OBJECT
public:
    explicit TextureWidget(QWidget *parent = nullptr);

    void present(const av::frame& frame);

    void present(const QPixmap& pixmap);

    void present(const QImage& image);

signals:
    void arrived();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    std::mutex mtx_;
    QPixmap    frame_{};
};

#endif // !CAPTURER_TEXTURE_WIDGET_H
