#include "framelesswindow.h"

#include "logging.h"

#include <optional>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QWindow>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windowsx.h>
#endif

FramelessWindow::FramelessWindow(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint | f)
{
#ifdef Q_OS_WIN
    auto hwnd = reinterpret_cast<HWND>(winId());

    // shadow
    DWMNCRENDERINGPOLICY ncrp = DWMNCRP_ENABLED;
    ::DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &ncrp, sizeof(DWMNCRENDERINGPOLICY));
    MARGINS margins = { -1, -1, -1, -1 };
    ::DwmExtendFrameIntoClientArea(hwnd, &margins);

    // window animation
    auto style = ::GetWindowLong(hwnd, GWL_STYLE);
    ::SetWindowLong(hwnd, GWL_STYLE,
                    style | WS_CAPTION | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME | CS_DBLCLKS);
#endif
}

void FramelessWindow::maximize(bool state) { state ? showMaximized() : showNormal(); }

void FramelessWindow::minimize(bool state) { state ? showMinimized() : showNormal(); }

void FramelessWindow::fullscreen(bool state) { state ? showFullScreen() : showNormal(); }

void FramelessWindow::mousePressEvent(QMouseEvent *event)
{
    if (!isFullScreen()) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
        windowHandle()->startSystemMove();
#elif defined(Q_OS_WIN)
        if (ReleaseCapture())
            SendMessage(reinterpret_cast<HWND>(winId()), WM_SYSCOMMAND, SC_MOVE + HTCAPTION, 0);
#elif defined(Q_OS_LINUX)
        if (event->button() == Qt::LeftButton) {
            moving_begin_ = event->globalPos() - pos();
        }
#endif
    }
    return QWidget::mousePressEvent(event);
}

void FramelessWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (!isFullScreen()) {
#ifdef Q_OS_LINUX
        if (event->buttons() & Qt::LeftButton) {
            move(event->globalPos() - moving_begin_);
        }
#endif
    }
    return QWidget::mouseMoveEvent(event);
}

void FramelessWindow::mouseReleaseEvent(QMouseEvent *event) { return QWidget::mouseReleaseEvent(event); }

void FramelessWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    return QWidget::mouseDoubleClickEvent(event);
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

    auto monitor = MonitorInfoFromWindow(hwnd);
    return monitor && (monitor->rcMonitor == winrect);
}
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool FramelessWindow::nativeEvent(const QByteArray& eventType, void *message, qintptr *result)
#else
bool FramelessWindow::nativeEvent(const QByteArray& eventType, void *message, long *result)
#endif
{
#ifdef Q_OS_WIN
    if (!message || !result || !reinterpret_cast<MSG *>(message)->hwnd) return false;

    auto wmsg = reinterpret_cast<MSG *>(message);
    auto hwnd = wmsg->hwnd;

    if (!hwnd) return false;

    switch (wmsg->message) {
    case WM_NCCALCSIZE: {
        const auto rect = ((static_cast<BOOL>(wmsg->wParam) == FALSE)
                               ? reinterpret_cast<LPRECT>(wmsg->lParam)
                               : &(reinterpret_cast<LPNCCALCSIZE_PARAMS>(wmsg->lParam))->rgrc[0]);

        auto max  = IsMaximized(hwnd);
        auto full = IsFullscreen(hwnd);

        const auto xthickness = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
        const auto ythickness = GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);

        if (max && !full) {
            rect->top += ythickness;
            rect->bottom -= ythickness;
            rect->left += xthickness;
            rect->right -= xthickness;
        }

        if (max || full) {
            APPBARDATA abd{ .cbSize = sizeof(APPBARDATA) };
            const UINT taskbar_state = SHAppBarMessage(ABM_GETSTATE, &abd);

            if (taskbar_state & ABS_AUTOHIDE) {

                UINT taskbar_postion = ABE_BOTTOM;

                auto monitor = MonitorInfoFromWindow(hwnd);
                if (!monitor) break;

                for (const auto& abe : std::vector<UINT>{ ABE_BOTTOM, ABE_TOP, ABE_LEFT, ABE_RIGHT }) {
                    APPBARDATA abd{ .cbSize = sizeof(APPBARDATA), .uEdge = abe, .rc = monitor->rcMonitor };
                    if (SHAppBarMessage(ABM_GETAUTOHIDEBAREX, &abd)) {
                        taskbar_postion = abe;
                        break;
                    }
                }

                switch (taskbar_postion) {
                case ABE_TOP: rect->top += 2; break;
                case ABE_LEFT: rect->left += 2; break;
                case ABE_RIGHT: rect->right -= 2; break;
                default: rect->bottom -= 2; break;
                }
            }
        }

        *result = (static_cast<BOOL>(wmsg->wParam) == FALSE) ? 0 : WVR_REDRAW;
        return true;
    }

        // resize
    case WM_NCHITTEST: {
        if (IsFullscreen(hwnd)) {
            *result = HTCLIENT;
            return true;
        }

        RECT rect{};
        if (!::GetWindowRect(hwnd, &rect)) return false;

        auto x = GET_X_LPARAM(wmsg->lParam);
        auto y = GET_Y_LPARAM(wmsg->lParam);

        const auto xthickness = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
        const auto ythickness = GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);

        auto hl = x >= rect.left && x < rect.left + xthickness;
        auto ht = y >= rect.top && y < rect.top + ythickness;
        auto hr = x >= rect.right - xthickness && x < rect.right;
        auto hb = y >= rect.bottom - ythickness && y < rect.bottom;

        if (hl && ht) {
            *result = HTTOPLEFT;
            return true;
        }

        if (hr && ht) {
            *result = HTTOPRIGHT;
            return true;
        }

        if (hl && hb) {
            *result = HTBOTTOMLEFT;
            return true;
        }

        if (hr && hb) {
            *result = HTBOTTOMRIGHT;
            return true;
        }

        if (hl) {
            *result = HTLEFT;
            return true;
        }

        if (hr) {
            *result = HTRIGHT;
            return true;
        }

        if (ht) {
            *result = HTTOP;
            return true;
        }
        if (hb) {
            *result = HTBOTTOM;
            return true;
        }

        break;
    }

    default: break;
    }
#endif

    return QWidget::nativeEvent(eventType, message, result);
}
