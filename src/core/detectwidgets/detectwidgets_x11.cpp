#ifdef __linux__
#include "detectwidgets.h"
#include <QCursor>
#include <QGuiApplication>
#include <QScreen>
#include <QX11Info>
#include <X11/Xlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include "displayinfo.h"

std::vector<std::pair<QString, QRect>> DetectWidgets::windows_;

void DetectWidgets::reset()
{
    windows_.clear();

    auto display = QX11Info::display();
    auto root_window = DefaultRootWindow(display);

    Window root_return, parent_return;
    Window *child_windows = nullptr;
    unsigned int child_num = 0;
    XQueryTree(display, root_window, &root_return, &parent_return, &child_windows, &child_num);

    for(unsigned int i = 0; i < child_num; ++i) {
        XWindowAttributes attrs;
        XGetWindowAttributes(display, child_windows[i], &attrs);
        char * buffer;
        XFetchName(display, child_windows[i], &buffer);
        auto name = QString(buffer);

        QRect rect(attrs.x, attrs.y, attrs.width, attrs.height);

        windows_.push_back({name, rect});
    }

    XFree(child_windows);
}

QRect DetectWidgets::window()
{
    QRect fullscreen({ 0, 0 }, DisplayInfo::instance().maxSize());
    auto cpos = QCursor::pos();
    QRect result = fullscreen;

    for(const auto& window: windows_) {
        if(window.second.contains(cpos)
                && (result.width() * result.height() > window.second.width() * window.second.height())) {
            result = fullscreen.intersected(window.second);
        }
    }

    return result;
}
#endif