#ifndef WIDGETS_DETECTOR_H
#define WIDGETS_DETECTOR_H

#include <QRect>
#include <vector>
#include <tuple>

class WidgetsDetector
{
public:
    static std::tuple<QString, QRect, uint64_t> window();       // window rect, name and id
    static QRect window_rect();
    static QRect widget();
    static void refresh();
private:
    static std::vector<std::tuple<QString, QRect, uint64_t>> windows_;
};

#endif // WIDGETS_DETECTOR_H
