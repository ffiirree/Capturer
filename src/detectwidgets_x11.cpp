#include "detectwidgets.h"
#include <QCursor>
#include <QGuiApplication>
#include <QScreen>

QRect DetectWidgets::window()
{
    QRect resoult = QGuiApplication::primaryScreen()->geometry();
    auto cpos = QCursor::pos();

    return resoult;
}
