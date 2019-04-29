#include "detectwidgets.h"
#include <Windows.h>
#include <QCursor>
#include <QGuiApplication>
#include <QScreen>
#include "Dwmapi.h"

QRect DetectWidgets::display()
{
    auto screens = QGuiApplication::screens();
    int width = 0, height = 0;
    foreach(const auto& screen, screens) {
        auto geometry = screen->geometry();
        width += geometry.width();
        if(height < geometry.height()) height = geometry.height();
    }

    return QRect(0, 0, width, height);
}

QRect DetectWidgets::window()
{
    auto resoult = display();

    auto cpos = QCursor::pos();

    auto hwnd = GetDesktopWindow();
    hwnd = GetWindow(hwnd, GW_CHILD);

    while (hwnd != NULL) {
        RECT rect;
        // https://stackoverflow.com/questions/34583160/winapi-createwindow-function-creates-smaller-windows-than-set
        // GetWindowRect(hwnd, &rect);
        DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(RECT));
        QRect window(QPoint(rect.left, rect.top),  QPoint(rect.right - 1, rect.bottom - 1));

        if(IsWindowVisible(hwnd) && window.contains(cpos)) {
            if(resoult.width() * resoult.height() > window.width() * window.height()) {
                resoult = window;
            }
        }

        hwnd = GetNextWindow(hwnd, GW_HWNDNEXT);
    }
    return resoult;
}
