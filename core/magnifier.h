#ifndef MAGNIFIER_H
#define MAGNIFIER_H

#include <QWidget>
#include <QLabel>
#include <QPixmap>

class Magnifier : public QFrame
{
public:
    explicit Magnifier(QWidget *parent = nullptr);

    inline void area(const QPixmap& a) { area_ = a; }

protected:
    void paintEvent(QPaintEvent *);

private:
    QLabel * label_ = nullptr;
    QPixmap area_;
};

#endif // MAGNIFIER_H
