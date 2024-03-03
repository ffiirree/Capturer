#ifndef CAPTURER_SCREENCAPTURER_H
#define CAPTURER_SCREENCAPTURER_H

#include "libcap/ffmpeg-wrapper.h"
#include "libcap/producer.h"

constexpr inline int CAPTURE_DESKTOP = 0x01;
constexpr inline int CAPTURE_DISPLAY = 0x02;
constexpr inline int CAPTURE_WINDOW  = 0x03;

class ScreenCapturer : public Producer<av::frame>
{
public:
    [[nodiscard]] bool is_realtime() const override { return true; }

    [[nodiscard]] bool has(AVMediaType mt) const override { return mt == AVMEDIA_TYPE_VIDEO; }

    // 1. framerate: vfmt.framerate
    // 2. region
    int      left; // + vfmt.width
    int      top;  // + vfmt.height
    // 3.
    bool     draw_cursor{ true };
    // 4.
    bool     show_region{};
    // 5.
    int      level{ CAPTURE_DESKTOP };
    uint64_t handle{}; // Windows: HMONITOR or HWND; Linux: X11 Window
};

#endif //! CAPTURER_SCREENCAPTURER_H
