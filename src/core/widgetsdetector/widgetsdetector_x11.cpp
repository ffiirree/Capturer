#ifdef __linux__
#include "widgetsdetector.h"
#include <QCursor>
#include <QGuiApplication>
#include <QScreen>
#include <QX11Info>
#include <X11/Xlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include "displayinfo.h"

std::vector<std::tuple<QString, QRect, uint64_t>> WidgetsDetector::windows_;

void WidgetsDetector::refresh()
{
    windows_.clear();

    auto display = QX11Info::display();
    auto root_window = DefaultRootWindow(display);

    Window root_return, parent_return;
    Window* child_windows = nullptr;
    unsigned int child_num = 0;
    XQueryTree(display, root_window, &root_return, &parent_return, &child_windows, &child_num);

    for (unsigned int i = 0; i < child_num; ++i) {
        XWindowAttributes attrs;
        XGetWindowAttributes(display, child_windows[i], &attrs);
        if (attrs.map_state >= 2) { // IsViewable
            char* buffer;
            XFetchName(display, child_windows[i], &buffer);

            QRect rect(attrs.x, attrs.y, attrs.width, attrs.height);
            windows_.push_back({ QString(buffer), rect, child_windows[i] });
        }
    }

    XFree(child_windows);
}

std::tuple<QString, QRect, uint64_t> WidgetsDetector::window()
{
    QRect fullscreen({ 0, 0 }, DisplayInfo::instance().maxSize());
    auto cpos = QCursor::pos();
    std::tuple<QString, QRect, uint64_t> window("desktop", fullscreen, 0);

    QRect rect = fullscreen;
    for (const auto& [wname, wrect, wid] : windows_) {
        if (wrect.contains(cpos) && (rect.width() * rect.height() > wrect.width() * wrect.height())) {
            rect = fullscreen.intersected(wrect);
            window = { wname, rect, wid };
        }
    }

    return window;
}

QRect WidgetsDetector::window_rect()
{
    return std::get<1>(window());
}
#endif