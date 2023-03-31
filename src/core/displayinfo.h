#ifndef DISPLAYINFO_H
#define DISPLAYINFO_H

#include <QScreen>

class DisplayInfo
{
public:
    DisplayInfo() = delete;
    DisplayInfo(const DisplayInfo&&) = delete;
    DisplayInfo& operator=(const DisplayInfo&) = delete;
    DisplayInfo(DisplayInfo&&) = delete;

    static QRect virtual_geometry();

    static QList<QScreen*> screens();
};

#endif // DISPLAYINFO_H
