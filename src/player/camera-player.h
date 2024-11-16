#ifndef CAPTURER_VIDEO_PLAYER_H
#define CAPTURER_VIDEO_PLAYER_H

#include "framelesswindow.h"
#include "libcap/producer.h"
#include "libcap/queue.h"
#include "texture-widget-rhi.h"

class QMenu;

class CameraPlayer final : public FramelessWindow
{
    Q_OBJECT

public:
    explicit CameraPlayer(QWidget *parent = nullptr);

    ~CameraPlayer() override;

    // open the file & playback
    int open(const std::string& filename, std::map<std::string, std::string> options);

    // do not call this function manually, called by dispatcher automatically
    int start();

    av::vformat_t vfmt{};

protected:
    void contextMenuEvent(QContextMenuEvent *) override;

private:
    std::string device_id_{};

    // UI
    QMenu            *menu_{};
    TextureRhiWidget *texture_{};

    std::atomic<bool> ready_{};
    std::atomic<bool> running_{};

    // video
    std::jthread                         thread_{};
    std::unique_ptr<Producer<av::frame>> source_{};
    safe_queue<av::frame>                vbuffer_{ 4 };
};

#endif // !CAPTURER_VIDEO_PLAYER_H