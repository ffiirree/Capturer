#ifndef ICON_BUTTON_H
#define ICON_BUTTON_H

#include "utils.h"
#include "custombutton.h"

class IconButton : public CustomButton
{
public:
    explicit IconButton(const QPixmap &, QWidget *parent = nullptr);
    IconButton(const QPixmap &, const QSize&, const QSize&, bool checkable = false, QWidget *parent = nullptr);

    void paint(QPainter *) override;
private:
    QPixmap icon_;
    QSize isize_;
};

#endif // ICON_BUTTON_H
