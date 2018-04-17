#include "detectwidgets.h"
#include <QCursor>
#include <QGuiApplication>
#include <QScreen>
#include <QX11Info>
#include <X11/Xlib.h>


QRect DetectWidgets::window()
{
    QRect resoult = QGuiApplication::primaryScreen()->geometry();
    auto cpos = QCursor::pos();

    auto display = QX11Info::display();
    auto root_window = DefaultRootWindow(display);

    Window root_return, parent_return;
    Window *child_windows = nullptr;
    unsigned int child_num = 0;
    XQueryTree(display, root_window, &root_return, &parent_return, &child_windows, &child_num);

    for(unsigned int i = 0; i < child_num; ++i) {
        XWindowAttributes attrs;
        XGetWindowAttributes(display, child_windows[i], &attrs);

        QRect rect{ attrs.x, attrs.y, attrs.width, attrs.height };

        if(rect.contains(cpos)) {
            if(resoult.width() * resoult.height() > rect.width() * rect.height()) {
                resoult = rect;
            }
        }
    }

    XFree(child_windows);

    return resoult;
}
