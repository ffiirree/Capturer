#include "framelesswindow.h"

#include "logging.h"
#include "platforms/window-effect.h"
#include "titlebar.h"

#include <QMouseEvent>
#include <QStyle>
#include <QWindow>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <optional>
#include <probe/graphics.h>
#include <windowsx.h>
#endif

// [microsoft/terminal](https://github.com/microsoft/terminal/blob/main/src/cascadia/WindowsTerminal/NonClientIslandWindow.cpp)
FramelessWindow::FramelessWindow(QWidget *parent, const Qt::WindowFlags flags)
    : QWidget(parent, Qt::Window | Qt::WindowCloseButtonHint | flags)
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
    ::SetWindowLong(hwnd, GWL_STYLE, ::GetWindowLong(hwnd, GWL_STYLE) & ~WS_SYSMENU);

    ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                   SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER |
                       SWP_FRAMECHANGED);
#endif

#ifdef Q_OS_LINUX
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
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

TitleBar *FramelessWindow::titlebar()
{
    if (!titlebar_) titlebar_ = new TitleBar(this);

    return titlebar_;
}

void FramelessWindow::toggleTransparentInput()
{
    transparent_input_ = !transparent_input_;

    TransparentInput(this, transparent_input_);
}

void FramelessWindow::mousePressEvent(QMouseEvent *event)
{
    if (dragmove_ && !isFullScreen() && event->button() == Qt::LeftButton) {
        dragmove_status_ = 1;
        return;
    }

    QWidget::mousePressEvent(event);
}

void FramelessWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (dragmove_status_ == 1) {
        dragmove_status_ = 2;
        windowHandle()->startSystemMove();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void FramelessWindow::mouseReleaseEvent(QMouseEvent *event)
{
    dragmove_status_ = 0;
    QWidget::mouseReleaseEvent(event);
}

void FramelessWindow::closeEvent(QCloseEvent *event)
{
    emit closed();
    QWidget::closeEvent(event);
}

void FramelessWindow::hideEvent(QHideEvent *event)
{
    emit hidden();
    QWidget::hideEvent(event);
}

#ifdef Q_OS_LINUX
void FramelessWindow::updateCursor(const Qt::Edges edges)
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
            windowHandle()->startSystemResize(edges_);
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

static int ResizeHandleHeight(HWND hWnd)
{
    const auto dpi = probe::graphics::retrieve_dpi_for_window(reinterpret_cast<uint64_t>(hWnd));

    return ::GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi) + ::GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
}

bool FramelessWindow::nativeEvent(const QByteArray& eventType, void *message, Q_NATIVE_EVENT_RESULT *result)
{
    if (!message || !result || !static_cast<MSG *>(message)->hwnd) return false;

    const auto wmsg = static_cast<MSG *>(message);
    const auto hwnd = wmsg->hwnd;

    if (!hwnd) return false;

    switch (wmsg->message) {
    // https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-nccalcsize
    case WM_NCCALCSIZE: {
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
            rect->top += ResizeHandleHeight(hwnd);
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
        LRESULT res = ::DefWindowProcW(hwnd, WM_NCHITTEST, 0, wmsg->lParam);
        if (res == HTCLIENT) {
            RECT rect{};
            if (::GetWindowRect(hwnd, &rect) &&
                GET_Y_LPARAM(wmsg->lParam) < rect.top + ResizeHandleHeight(hwnd)) {
                res = HTTOP;
            }
        }

        if (IsFullscreen(hwnd) || IsMaximized(hwnd) || isSizeFixed()) {
            switch (res) {
            case HTTOP:
            case HTRIGHT:
            case HTLEFT:
            case HTBOTTOM:
            case HTTOPLEFT:
            case HTTOPRIGHT:
            case HTBOTTOMLEFT:
            case HTBOTTOMRIGHT: res = HTCLIENT; break;
            default:            break;
            }
        }

        if (res == HTCLIENT && titlebar_ && titlebar_->isVisible()) {
            if (const auto pos = mapFromGlobal({ GET_X_LPARAM(wmsg->lParam), GET_Y_LPARAM(wmsg->lParam) });
                titlebar_->geometry().contains(pos) && !titlebar_->isInSystemButtons(pos)) {
                *result = HTCAPTION;
                return true;
            }
        }

        *result = static_cast<long>(res);
        return true;
    }

    default: break;
    }

    return QWidget::nativeEvent(eventType, message, result);
}
#endif

