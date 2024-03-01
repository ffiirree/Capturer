#include "window-effect.h"

#if __linux__

#include <QtX11Extras/QX11Info>
#include <QWidget>
#include <X11/extensions/shape.h>

void TransparentInput(QWidget *win, const bool en)
{
    if (en) {
        ::XShapeCombineRectangles(QX11Info::display(), win->winId(), ShapeInput, 0, 0, nullptr, 0, ShapeSet,
                                  YXBanded);
        return;
    }

    // FIXME: not working after window resizing
    XRectangle rect{
        .x      = 0,
        .y      = 0,
        .width  = static_cast<unsigned short>(win->width()),
        .height = static_cast<unsigned short>(win->height()),
    };

    ::XShapeCombineRectangles(QX11Info::display(), win->winId(), ShapeInput, 0, 0, &rect, 1, ShapeSet,
                              YXBanded);

    win->activateWindow();
}

#endif