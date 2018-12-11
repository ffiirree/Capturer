#ifndef DECTE_H
#define DECTE_H

#include <QRect>

class DetectWidgets
{
public:
    static QRect display();

    static QRect window();
    static QRect widget();
private:
};

#endif // DECTE_H
