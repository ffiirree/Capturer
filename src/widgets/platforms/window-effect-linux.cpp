#include "window-effect.h"

#if __linux__

#include <QGuiApplication>
#include <QWidget>
#include <X11/extensions/shape.h>

using namespace QNativeInterface;

void TransparentInput(QWidget *win, const bool en)
{
    const auto x11app = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();

    if (!x11app) return;

    if (en) {
        ::XShapeCombineRectangles(x11app->display(), win->winId(), ShapeInput, 0, 0, nullptr, 0, ShapeSet,
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

    ::XShapeCombineRectangles(x11app->display(), win->winId(), ShapeInput, 0, 0, &rect, 1, ShapeSet,
                              YXBanded);

    win->activateWindow();
}

#endif