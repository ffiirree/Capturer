#ifdef _WIN32
#include "widgetsdetector.h"
#include <Windows.h>
#include <QCursor>
#include <QGuiApplication>
#include <QScreen>
#include "dwmapi.h"
#include "displayinfo.h"

std::vector<std::tuple<QString, QRect, uint64_t>> WidgetsDetector::windows_;

QRect getRect(HWND hWnd)
{
    RECT rect;
    // https://stackoverflow.com/questions/34583160/winapi-createwindow-function-creates-smaller-windows-than-set
    // GetWindowRect(hwnd, &rect);
    DwmGetWindowAttribute(hWnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(RECT));
    return QRect(QPoint(rect.left, rect.top),  QPoint(rect.right - 1, rect.bottom - 1));
}

 QString getName(HWND hWnd)
 {
     WCHAR buffer[255];
     GetWindowText(hWnd, buffer, 255);
     return QString::fromWCharArray(buffer);
 }

void WidgetsDetector::refresh()
{
    windows_.clear();

    // Z-order
    auto hwnd = GetTopWindow(nullptr);
    while (hwnd != nullptr) {
        auto rect = getRect(hwnd);

        if(IsWindowVisible(hwnd) && rect.isValid()) {
            windows_.push_back({ getName(hwnd), rect, (uint64_t)hwnd });
        }

        hwnd = GetNextWindow(hwnd, GW_HWNDNEXT);        // Z-index: up to down
    }
}

std::tuple<QString, QRect, uint64_t> WidgetsDetector::window()
{
    QRect fullscreen(DisplayInfo::virutal_geometry());
    auto cpos = QCursor::pos();

    for (const auto& [wname, wrect, wid] : windows_) {
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