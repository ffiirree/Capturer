#ifdef __APPLE__
#include "widgetsdetector.h"
#include <QCursor>
#include <QGuiApplication>
#include <QScreen>

std::vector<std::tuple<QString, QRect, uint64_t>> WidgetsDetector::windows_;

std::tuple<QString, QRect, uint64_t> WidgetsDetector::window()
{
    return { "desktop", QGuiApplication::primaryScreen()->geometry(), 0 };
}
#endif