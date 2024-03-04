#include "framelesswindow.h"

#include "logging.h"
#include "platforms/window-effect.h"

#include <QMouseEvent>
#include <QStyle>
#include <QWindow>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <optional>
#include <probe/graphics.h>
#include <windowsx.h>
#endif

FramelessWindow::FramelessWindow(QWidget *parent, const Qt::WindowFlags flags)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint | Qt::WindowCloseButtonHint | flags)
{
#ifdef Q_OS_WIN
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    setAttribute(Qt::WA_NativeWindow);

    const auto hwnd = reinterpret_cast<HWND>(winId());

    // shadow
    constexpr DWMNCRENDERINGPOLICY ncrp = DWMNCRP_ENABLED;
    ::DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &ncrp, sizeof(DWMNCRENDERINGPOLICY));

    constexpr MARGINS margins = { -1, -1, -1, -1 };
    ::DwmExtendFrameIntoClientArea(hwnd, &margins);

    // Window Styles: https://learn.microsoft.com/en-us/windows/win32/winmsg/window-styles
    auto style = ::GetWindowLong(hwnd, GWL_STYLE);
    if (windowFlags() & Qt::WindowTitleHint) style |= WS_CAPTION | WS_THICKFRAME;
    ::SetWindowLong(hwnd, GWL_STYLE, style & ~WS_SYSMENU);

    ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                   SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER |
                       SWP_FRAMECHANGED);
#endif

#ifdef Q_OS_LINUX
    setAttribute(Qt::WA_Hover, true);
#endif
}

bool FramelessWindow::isSizeFixed() const
{
    if (windowFlags() & Qt::MSWindowsFixedSizeDialogHint) {
        return true;
    }

    const QSize minsize = minimumSize();
    const QSize maxsize = maximumSize();

    return !minsize.isEmpty() && !maxsize.isEmpty() && (minsize == maxsize);
}

void FramelessWindow::maximize(const bool state) { state ? showMaximized() : showNormal(); }

void FramelessWindow::toggleMaximized() { !isMaximized() ? showMaximized() : showNormal(); }

void FramelessWindow::minimize(const bool state) { state ? showMinimized() : showNormal(); }

void FramelessWindow::fullscreen(const bool state) { state ? showFullScreen() : showNormal(); }

void FramelessWindow::toggleFullScreen() { isFullScreen() ? showNormal() : showFullScreen(); }

void FramelessWindow::toggleTransparentInput()
{
    transparent_input_ = !transparent_input_;

    TransparentInput(this, transparent_input_);
}

void FramelessWindow::mousePressEvent(QMouseEvent *event)
{
    if (!isFullScreen() && event->button() == Qt::LeftButton) {
        windowHandle()->startSystemMove();
        return;
    }

    return QWidget::mousePressEvent(event);
}

void FramelessWindow::closeEvent(QCloseEvent *event)
{
    emit closed();
    return QWidget::closeEvent(event);
}

void FramelessWindow::hideEvent(QHideEvent *event)
{
    emit hidden();
    return QWidget::hideEvent(event);
}

void FramelessWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        if (windowState() == Qt::WindowNoState) emit normalized();
        if (windowState() & Qt::WindowMinimized) emit minimized();
        if (windowState() & Qt::WindowMaximized) emit maximized();
        if (windowState() & Qt::WindowFullScreen) emit fullscreened();
    }
    QWidget::changeEvent(event);
}

#ifdef Q_OS_LINUX
void FramelessWindow::updateCursor(Qt::Edges edges)
{
    switch (edges) {
    case Qt::LeftEdge:
    case Qt::RightEdge:                  setCursor(Qt::SizeHorCursor); break;
    case Qt::TopEdge:
    case Qt::BottomEdge:                 setCursor(Qt::SizeVerCursor); break;
    case Qt::LeftEdge | Qt::TopEdge:
    case Qt::RightEdge | Qt::BottomEdge: setCursor(Qt::SizeFDiagCursor); break;
    case Qt::RightEdge | Qt::TopEdge:
    case Qt::LeftEdge | Qt::BottomEdge:  setCursor(Qt::SizeBDiagCursor); break;
    default:                             setCursor(Qt::ArrowCursor); break;
    }
}

// FIXME:
bool FramelessWindow::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        if (edges_) {
            window()->windowHandle()->startSystemResize(edges_);
            return true;
        }
        break;
    case QEvent::MouseButtonRelease: break;
    case QEvent::HoverEnter:
    case QEvent::HoverLeave:
    case QEvent::HoverMove:          {
        const auto pos = dynamic_cast<QHoverEvent *>(event)->pos();
        const auto ftn = style()->pixelMetric(QStyle::PM_LayoutBottomMargin);

        edges_ = Qt::Edges{};

        if (pos.x() < ftn) edges_ |= Qt::LeftEdge;
        if (pos.x() > width() - ftn) edges_ |= Qt::RightEdge;
        if (pos.y() < ftn) edges_ |= Qt::TopEdge;
        if (pos.y() > height() - ftn) edges_ |= Qt::BottomEdge;

        updateCursor(edges_);
        break;
    }
    default: break;
    }

    return QWidget::event(event);
}
#endif

#ifdef Q_OS_WIN
static bool operator==(const RECT& lhs, const RECT& rhs) noexcept
{
    return ((lhs.left == rhs.left) && (lhs.top == rhs.top) && (lhs.right == rhs.right) &&
            (lhs.bottom == rhs.bottom));
}

static std::optional<MONITORINFOEX> MonitorInfoFromWindow(HWND hwnd)
{
    if (!hwnd) return std::nullopt;
    HMONITOR hmonitor = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

    MONITORINFOEX info = { sizeof(MONITORINFOEX) };
    if (::GetMonitorInfo(hmonitor, &info)) return info;
    return std::nullopt;
}

static bool IsFullscreen(HWND hwnd)
{
    if (!hwnd) return false;

    RECT winrect = {};
    if (::GetWindowRect(hwnd, &winrect) == FALSE) return false;

    const auto monitor = MonitorInfoFromWindow(hwnd);
    return monitor && (monitor->rcMonitor == winrect);
}

bool FramelessWindow::nativeEvent(const QByteArray& eventType, void *message, Q_NATIVE_EVENT_RESULT *result)
{
    if (!message || !result || !static_cast<MSG *>(message)->hwnd) return false;

    const auto wmsg = static_cast<MSG *>(message);
    const auto hwnd = wmsg->hwnd;

    if (!hwnd) return false;

    switch (wmsg->message) {
    case WM_NCCALCSIZE: {
        // https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-nccalcsize
        // Parameters
        // wParam
        //
        // If wParam is TRUE, it specifies that the application should indicate which part of the client
        // area contains valid information. The system copies the valid information to the specified area
        // within the new client area.
        //
        // If wParam is FALSE, the application does not need to indicate the valid part of the client area.
        //
        // lParam
        //
        // If wParam is TRUE, lParam points to an NCCALCSIZE_PARAMS structure that contains information an
        // application can use to calculate the new size and position of the client rectangle.
        //
        // If wParam is FALSE, lParam points to a RECT structure. On entry, the structure contains the
        // proposed window rectangle for the window. On exit, the structure should contain the screen
        // coordinates of the corresponding window client area.
        //
        // Return value
        //
        // If the wParam parameter is FALSE, the application should return zero.
        //
        // If wParam is TRUE, the application should return zero or a combination of the following values.
        //
        // If wParam is TRUE and an application returns zero, the old client area is preserved and is
        // aligned with the upper-left corner of the new client area.
        const auto rect = wmsg->wParam ? &(reinterpret_cast<LPNCCALCSIZE_PARAMS>(wmsg->lParam))->rgrc[0]
                                       : reinterpret_cast<LPRECT>(wmsg->lParam);

        const LONG original_top = rect->top;
        // apply the default frame for standard window frame (the resizable frame border and the frame
        // shadow) including the left, bottom and right edges.
        if (const LRESULT res = ::DefWindowProcW(hwnd, WM_NCCALCSIZE, wmsg->wParam, wmsg->lParam);
            (res != HTERROR) && (res != HTNOWHERE)) {
            *result = static_cast<long>(res);
            return true;
        }
        // re-apply the original top for removing the top frame entirely
        rect->top = original_top;

        //
        const auto monitor    = MonitorInfoFromWindow(hwnd);
        const auto fullscreen = monitor && (monitor->rcMonitor == *rect);
        const auto maximized  = IsMaximized(hwnd);

        // top frame
        if (maximized && !fullscreen) {
            const auto dpi = probe::graphics::retrieve_dpi_for_window(reinterpret_cast<uint64_t>(hwnd));

            rect->top += ::GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi) +
                         ::GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
        }

        // autohide taskbar
        if (maximized || fullscreen) {
            APPBARDATA abd{ .cbSize = sizeof(APPBARDATA) };

            if (const UINT taskbar_state = ::SHAppBarMessage(ABM_GETSTATE, &abd);
                taskbar_state & ABS_AUTOHIDE) {

                UINT taskbar_postion = ABE_BOTTOM;

                if (!monitor) break;

                for (const auto& abe : std::vector<UINT>{ ABE_BOTTOM, ABE_TOP, ABE_LEFT, ABE_RIGHT }) {
                    APPBARDATA pos{ .cbSize = sizeof(APPBARDATA), .uEdge = abe, .rc = monitor->rcMonitor };
                    if (::SHAppBarMessage(ABM_GETAUTOHIDEBAREX, &pos)) {
                        taskbar_postion = abe;
                        break;
                    }
                }

                switch (taskbar_postion) {
                case ABE_TOP:   rect->top += 2; break;
                case ABE_LEFT:  rect->left += 2; break;
                case ABE_RIGHT: rect->right -= 2; break;
                default:        rect->bottom -= 2; break;
                }
            }
        }

        *result = wmsg->wParam ? WVR_REDRAW : FALSE;
        return true;
    }

    case WM_NCHITTEST: {
        if (const LRESULT res = ::DefWindowProcW(hwnd, WM_NCHITTEST, 0, wmsg->lParam); res != HTCLIENT) {
            *result = static_cast<long>(res);
            return true;
        }

        // CLIENT
        if (IsFullscreen(hwnd) || IsMaximized(hwnd) || isSizeFixed()) {
            *result = HTCLIENT;
            return true;
        }

        // TOP
        RECT rect{};
        if (!::GetWindowRect(hwnd, &rect)) return false;

        const POINT pos{ GET_X_LPARAM(wmsg->lParam), GET_Y_LPARAM(wmsg->lParam) };
        const auto  dpi = probe::graphics::retrieve_dpi_for_window(reinterpret_cast<uint64_t>(hwnd));
        if (const auto frame = ::GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi) +
                               ::GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
            pos.y < rect.top + frame) {
            *result = HTTOP;
            return true;
        }

        break;
    }

    default: break;
    }

    return QWidget::nativeEvent(eventType, message, result);
}
#endif

