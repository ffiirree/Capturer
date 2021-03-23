#ifdef __APPLE__
#include "widgetsdetector.h"
#include <QCursor>
#include <QGuiApplication>
#include <QScreen>

QRect WidgetsDetector::window()
{
    QRect resoult = QGuiApplication::primaryScreen()->geometry();
    auto cpos = QCursor::pos();

    return resoult;
}
#endif