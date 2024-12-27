#include "menu.h"

#include "platforms/window-effect.h"

#include <QEvent>
#include <QPlatformSurfaceEvent>
#include <QStyleHints>

Menu::Menu(QWidget *parent)
    : Menu({}, parent)
{}

Menu::Menu(const QString& title, QWidget *parent)
    : QMenu(title, parent)
{
    setWindowFlag(Qt::NoDropShadowWindowHint);

#ifdef Q_OS_LINUX
    setProperty("system", "linux");
#endif

#ifdef Q_OS_WIN
    installEventFilter(this);
#endif
}

#ifdef Q_OS_WIN
bool Menu::eventFilter(QObject *, QEvent *event)
{
    if (event->type() == QEvent::PlatformSurface) {
        if (dynamic_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType() !=
            QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
            const auto hwnd = reinterpret_cast<HWND>(winId());
            windows::dwm::set_window_corner(hwnd, DWMWCP_ROUND);
            windows::dwm::set_material_acrylic(
                hwnd,
                qApp->styleHints()->colorScheme() == Qt::ColorScheme::Light ? 0xBBFFFFFF : 0xBB000000);
        }
    }
    return false;
}
#endif