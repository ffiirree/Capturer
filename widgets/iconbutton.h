#ifndef ICONBUTTON_H
#define ICONBUTTON_H

#include "utils.h"
#include "custombutton.h"

class IconButton : public CustomButton
{
public:
    IconButton(const QPixmap &, QWidget *parent = nullptr);
    IconButton(const QPixmap &, const QSize&, const QSize&, bool checkable = false, QWidget *parent = nullptr);

    virtual void paint(QPainter *) override;
private:
    QPixmap icon_;
    QSize isize_;
};

#endif // ICONBUTTON_H
