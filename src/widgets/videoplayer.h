#ifndef CAPTURER_VIDEO_PLAYER_H
#define CAPTURER_VIDEO_PLAYER_H

#include <QWidget>
#include "decoder.h"
#include "consumer.h"
#include "dispatcher.h"

class VideoPlayer : public QWidget, public Consumer<AVFrame> {
    Q_OBJECT

public:
    explicit VideoPlayer(QWidget* parent = nullptr);
    ~VideoPlayer() override;

    int run() override { return 0; };
    [[nodiscard]] bool ready() const override { return decoder_ && decoder_->ready(); }
    void reset() override { }

    int consume(AVFrame* frame, int type) override;

    [[nodiscard]] bool full(int) const override { return false; }
    void enable(int, bool) override { }
    [[nodiscard]] bool accepts(int type) const override { return type == AVMEDIA_TYPE_VIDEO; }

    bool play(const std::string& name, const std::string& fmt = "", const std::string& filters = "");

signals:
    void started();
    void closed();

protected:
    void paintEvent(QPaintEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

    Decoder* decoder_{ nullptr };
    Dispatcher* dispatcher_{ nullptr };

    std::mutex mtx_;
    AVFrame* frame_{ nullptr };
};

#endif // !CAPTURER_VIDEO_PLAYER_H