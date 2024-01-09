#ifndef CAPTURER_TEXTURE_WIDGET_H
#define CAPTURER_TEXTURE_WIDGET_H

#include <mutex>
#include <QWidget>

extern "C" {
#include <libavutil/frame.h>
}

class TextureWidget final : public QWidget
{
    Q_OBJECT
public:
    explicit TextureWidget(QWidget *parent = nullptr);

    void present(AVFrame *frame);

    void present(const QPixmap& pixmap);

    void present(const QImage& image);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    std::mutex mtx_;
    QPixmap frame_{};
};

#endif // !CAPTURER_TEXTURE_WIDGET_H
