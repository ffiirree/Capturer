#ifndef CAPTURER_VIDEO_PLAYER_H
#define CAPTURER_VIDEO_PLAYER_H

#include "consumer.h"
#include "decoder.h"
#include "dispatcher.h"

#include <QWidget>

class VideoPlayer : public QWidget, public Consumer<AVFrame>
{
    Q_OBJECT

public:
    explicit VideoPlayer(QWidget *parent = nullptr);
    ~VideoPlayer() override;

    int run() override { return 0; };

    [[nodiscard]] bool ready() const override { return decoder_ && decoder_->ready(); }

    void reset() override {}

    int consume(AVFrame *frame, int type) override;

    [[nodiscard]] bool full(int) const override { return false; }

    void enable(int, bool) override {}

    [[nodiscard]] bool accepts(int type) const override { return type == AVMEDIA_TYPE_VIDEO; }

    int open(const std::string&, std::map<std::string, std::string>) override;

signals:
    void started();
    void closed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

    Decoder *decoder_{ nullptr };
    Dispatcher *dispatcher_{ nullptr };

    std::mutex mtx_;
    AVFrame *frame_{ nullptr };
};

#endif // !CAPTURER_VIDEO_PLAYER_H