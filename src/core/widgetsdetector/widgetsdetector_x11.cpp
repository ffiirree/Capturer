#ifdef __linux__

#include "widgetsdetector.h"
#include <QCursor>
#include "platform.h"
#include <X11/Xlib.h>

std::vector<std::tuple<QString, QRect, uint64_t>> WidgetsDetector::windows_;

void WidgetsDetector::refresh()
{
    windows_.clear();

    auto display = XOpenDisplay(nullptr);
    if (!display) return;

    auto root_window = DefaultRootWindow(display);

    Window root_return, parent_return;
    Window* child_windows = nullptr;
    unsigned int child_num = 0;
    // The XQueryTree() function returns the root ID, the parent window ID, a pointer to the list of children windows (NULL when there are no children), 
    // and the number of children in the list for the specified window. 
    // The children are listed in current stacking order, from bottommost (first) to topmost (last). 
    // XQueryTree() returns zero if it fails and nonzero if it succeeds. 
    // To free a non-NULL children list when it is no longer needed, use XFree().
    XQueryTree(display, root_window, &root_return, &parent_return, &child_windows, &child_num);

    for (unsigned int i = 0; i < child_num; ++i) {
        XWindowAttributes attrs;
        XGetWindowAttributes(display, child_windows[i], &attrs);
        if (attrs.map_state >= 2) { // IsViewable
            char* buffer = nullptr;
            XFetchName(display, child_windows[i], &buffer);
            
            QRect rect(attrs.x, attrs.y, attrs.width, attrs.height);
            windows_.emplace_back(QString(buffer), rect, child_windows[i]);

            XFree(buffer);
        }
    }

    XFree(child_windows);
    XCloseDisplay(display);
}

std::tuple<QString, QRect, uint64_t> WidgetsDetector::window()
{
    QRect fullscreen(platform::display::virtual_screen_geometry());
    auto cpos = QCursor::pos();

    // listed in current stacking order, from bottommost (first) to topmost (last).
    for (auto first = windows_.rbegin(); first != windows_.rend(); ++first) {
        const auto& [wname, wrect, wid] = *first;
        if (wrect.contains(cpos)) {
            return { wname, fullscreen.intersected(wrect), wid };
        }
    }

    return { "desktop", fullscreen, 0 };
}

QRect WidgetsDetector::window_rect()
{
    return std::get<1>(window());
}
#endif