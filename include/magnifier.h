#ifndef MAGNIFIER_H
#define MAGNIFIER_H

#include <QWidget>
#include <QLabel>
#include <QPixmap>

class Magnifier : public QFrame
{
public:
    explicit Magnifier(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *);

private:
    QLabel * label_ = nullptr;
};

#endif // MAGNIFIER_H
