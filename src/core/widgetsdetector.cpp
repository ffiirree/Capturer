#include "widgetsdetector.h"
#include "platform.h"
#include "logging.h"

std::deque<platform::display::window_t> WidgetsDetector::windows_;

void WidgetsDetector::refresh()
{
    windows_ = platform::display::windows();
}

platform::display::window_t WidgetsDetector::window(const QPoint& pos)
{
    auto desktop = platform::display::virtual_screen_geometry();

    for (auto win : windows_) {
        if (win.rect.contains(pos.x(), pos.y())) {
            win.rect = desktop.intersected(win.rect);
            return win;
        }
    }

    return { "desktop", "Virtual-Screen", desktop, 0, true };
}