#ifndef ICON_BUTTON_H
#define ICON_BUTTON_H

#include "utils.h"
#include "custombutton.h"

class IconButton : public CustomButton
{
public:
    explicit IconButton(const QPixmap&, QWidget* = nullptr);
    IconButton(const QPixmap&, const QSize&, const QSize&, bool = false, QWidget* = nullptr);

    void paint(QPainter*) override;
private:
    QPixmap icon_;
    QSize isize_;
};

#endif // ICON_BUTTON_H
