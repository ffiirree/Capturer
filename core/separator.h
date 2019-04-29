#ifndef SEPARATOR_H
#define SEPARATOR_H

#include <QFrame>

class Separator : public QFrame
{
public:
    explicit Separator(QWidget *parent = nullptr);
    explicit Separator(Shape shape, int len, QWidget *parent = nullptr);
};

#endif // SEPARATOR_H
