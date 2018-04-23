#ifndef MAGNIFIER_H
#define MAGNIFIER_H

#include <QWidget>
#include <QLabel>
#include <QPixmap>

class Magnifier : public QFrame
{
public:
    explicit Magnifier(QWidget *parent = nullptr);

    inline void pixmap(const QPixmap& p) { pixmap_ = p; }
    QRect mrect();

protected:
    void paintEvent(QPaintEvent *);

private:
    QLabel * label_ = nullptr;
    QPixmap pixmap_;

    int alpha_ = 5;
    QSize msize_{ 31, 25 };
    QSize psize_{ 0, 0 };
};

#endif // MAGNIFIER_H
