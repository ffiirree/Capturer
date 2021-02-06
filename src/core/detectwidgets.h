#ifndef DETECT_WIDGETS_H
#define DETECT_WIDGETS_H

#include <QRect>
#include <vector>

class DetectWidgets
{
public:
    static QRect window();
    static QRect widget();
    static void reset();
private:
    static std::vector<std::pair<QString, QRect>> windows_;
};

#endif // DETECT_WIDGETS_H
