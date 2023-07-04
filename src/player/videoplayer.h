#ifndef CAPTURER_VIDEO_PLAYER_H
#define CAPTURER_VIDEO_PLAYER_H

#include "framelesswindow.h"
#include "libcap/consumer.h"
#include "libcap/decoder.h"
#include "libcap/dispatcher.h"
#include "texture-widget.h"

class VideoPlayer : public FramelessWindow, public Consumer<AVFrame>
{
    Q_OBJECT

public:
    explicit VideoPlayer(QWidget *parent = nullptr);
    ~VideoPlayer() override;

    int open(const std::string&, std::map<std::string, std::string>) override;

    int run() override { return 0; };

    [[nodiscard]] bool ready() const override { return decoder_ && decoder_->ready(); }

    void reset() override {}

    int consume(AVFrame *frame, int type) override;

    [[nodiscard]] bool full(int) const override { return false; }

    void enable(int, bool) override {}

    [[nodiscard]] bool accepts(int type) const override { return type == AVMEDIA_TYPE_VIDEO; }

signals:
    void started();
    void closed();

protected:
    void closeEvent(QCloseEvent *event) override;

    TextureWidget *texture_{};
    Decoder *decoder_{ nullptr };
    Dispatcher *dispatcher_{ nullptr };
};

#endif // !CAPTURER_VIDEO_PLAYER_H