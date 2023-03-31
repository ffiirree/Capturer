#include "displayinfo.h"
#include <QApplication>
#include "utils.h"

QList<QScreen*> DisplayInfo::screens()
{
    return QGuiApplication::screens();
}

QRect DisplayInfo::virtual_geometry()
{
    QRegion region{};

    foreach(const auto & screen, QGuiApplication::screens()) {
        region += screen->geometry();
    }

    return region.boundingRect();
}