#include "widgetsdetector.h"

#include "logging.h"
#include "probe/graphics.h"

std::deque<probe::graphics::window_t> WidgetsDetector::windows_;

void WidgetsDetector::refresh() { windows_ = probe::graphics::windows(); }

probe::graphics::window_t WidgetsDetector::window(const QPoint& pos)
{
    auto desktop = probe::graphics::virtual_screen_geometry();

    for(auto win : windows_) {
        if(win.rect.contains(pos.x(), pos.y())) {
            win.rect = desktop.intersected(win.rect);
            return win;
        }
    }

    return { "desktop", "Virtual-Screen", desktop, 0, true };
}