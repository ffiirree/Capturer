#ifndef CAPTURER_XSHM_CAPTURER_H
#define CAPTURER_XSHM_CAPTURER_H

#ifdef __linux__

#include "libcap/ffmpeg-wrapper.h"
#include "libcap/screen-capturer.h"

#include <thread>
#include <xcb/xcb.h>

class XshmCapturer final : public ScreenCapturer
{
public:
    ~XshmCapturer() override;

    int open(const std::string& name, std::map<std::string, std::string> options) override;

    int start() override;

    void stop() override;

    bool has(AVMediaType mt) const override;

private:
    int xfixes_draw_cursor(av::frame& frame) const;

    xcb_connection_t *conn_{};
    xcb_screen_t     *screen_{};
    xcb_window_t      wid_{};
    AVBufferPool     *xshm_pool_{};
    int               bpp_{};
    size_t            frame_size_{};

    std::jthread thread_{};
};

#endif

#endif // !CAPTURER_XSHM_CAPTURER_H