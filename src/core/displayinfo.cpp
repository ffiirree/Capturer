#include "displayinfo.h"
#include <QApplication>
#include "utils.h"
#include "logging.h"
#include "fmt/format.h"

QList<QScreen*> DisplayInfo::screens()
{
    return QGuiApplication::screens();
}

QRect DisplayInfo::virutal_geometry()
{
    QRegion region{};

    foreach(const auto & screen, QGuiApplication::screens()) {
        region += screen->geometry();
    }

    return region.boundingRect();
}