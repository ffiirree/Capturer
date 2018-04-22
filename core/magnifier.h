#ifndef MAGNIFIER_H
#define MAGNIFIER_H

#include <QWidget>
#include <QLabel>
#include <QPixmap>

class Magnifier : public QFrame
{
public:
    explicit Magnifier(QWidget *parent = nullptr);

    inline void pixmap(const QPixmap& a) { area_ = a; }

    QRect area();

protected:
    void paintEvent(QPaintEvent *);

private:
    QLabel * label_ = nullptr;
    QPixmap area_;

    int w_ = 31;
    int h_ = 25;
};

#endif // MAGNIFIER_H
