#ifndef WIDGETS_DETECTOR_H
#define WIDGETS_DETECTOR_H

#include <QPoint>
#include <deque>
#include "platform.h"

class WidgetsDetector
{
public:
    static platform::display::window_t window(const QPoint&);   // global pos
    static void refresh();

    WidgetsDetector() = delete;
    WidgetsDetector(const WidgetsDetector&) = delete;
    WidgetsDetector(WidgetsDetector&&) = delete;
    WidgetsDetector& operator=(const WidgetsDetector&) = delete;
    WidgetsDetector& operator=(WidgetsDetector&&) = delete;

private:
    static std::deque<platform::display::window_t> windows_;
};

#endif // WIDGETS_DETECTOR_H
