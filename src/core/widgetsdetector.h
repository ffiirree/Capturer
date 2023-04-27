#ifndef WIDGETS_DETECTOR_H
#define WIDGETS_DETECTOR_H

#include "probe/graphics.h"

#include <QPoint>
#include <deque>

class WidgetsDetector
{
public:
    static probe::graphics::window_t window(const QPoint&); // global pos
    static void refresh();

    WidgetsDetector()                                  = delete;
    WidgetsDetector(const WidgetsDetector&)            = delete;
    WidgetsDetector(WidgetsDetector&&)                 = delete;
    WidgetsDetector& operator=(const WidgetsDetector&) = delete;
    WidgetsDetector& operator=(WidgetsDetector&&)      = delete;

private:
    static std::deque<probe::graphics::window_t> windows_;
};

#endif // WIDGETS_DETECTOR_H
