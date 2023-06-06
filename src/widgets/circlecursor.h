#ifndef CAPTURER_CIRCLE_CURSOR_H
#define CAPTURER_CIRCLE_CURSOR_H

#include <QObject>
#include <QPixmap>

class CircleCursor : public QObject
{
    Q_OBJECT

public:
    CircleCursor();
    explicit CircleCursor(int width);

    void repaint();

    inline QPixmap cursor() const { return cursor_; }

    inline int width() const { return width_; }

public slots:
    void setWidth(int val)
    {
        width_ = val;
        width_ = std::min(val, 49);
        repaint();
    }

private:
    QPixmap cursor_{ 51, 51 };
    int width_{ 3 };
};

#endif //! CAPTURER_CIRCLE_CURSOR_H
