#ifndef CIRCLECURSOR_H
#define CIRCLECURSOR_H

#include <QPixmap>

class CircleCursor : QObject
{
    Q_OBJECT

public:
    CircleCursor();
    CircleCursor(int width);

    void repaint();

    inline QPixmap cursor() const { return cursor_; }
    inline int width() const { return width_; }

public slots:
    void setWidth(int val) { width_ = val; width_ = std::min(val, 50); repaint(); }

private:
    QPixmap cursor_{50, 50};
    int width_;
};

#endif // CIRCLECURSOR_H
