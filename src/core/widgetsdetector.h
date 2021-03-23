#ifndef WIDGETS_DETECTOR_H
#define WIDGETS_DETECTOR_H

#include <QRect>
#include <vector>

class WidgetsDetector
{
public:
    static QRect window();
    static QRect widget();
    static void reset();
private:
    static std::vector<std::pair<QString, QRect>> windows_;
};

#endif // WIDGETS_DETECTOR_H
