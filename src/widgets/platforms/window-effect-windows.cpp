#include "window-effect.h"

#if _WIN32

#include <probe/library.h>
#include <probe/system.h>
#include <QWidget>
#include <winuser.h>

#define DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 19

namespace windows::dwm
{
    HRESULT enable_shadow(HWND hwnd, bool en)
    {
        DWMNCRENDERINGPOLICY ncrp = en ? DWMNCRP_ENABLED : DWMNCRP_DISABLED;
        return ::DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &ncrp, sizeof(DWMNCRENDERINGPOLICY));
    }

    HRESULT set_window_corner(HWND hwnd, DWM_WINDOW_CORNER_PREFERENCE corner)
    {
        return ::DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner,
                                       sizeof(DWMWINDOWATTRIBUTE));
    }

    HRESULT blur_behind(HWND hwnd, BOOL en)
    {
        const DWM_BLURBEHIND bb{ 1, en, nullptr, FALSE };
        return ::DwmEnableBlurBehindWindow(hwnd, &bb);
    }

    HRESULT blur(HWND hwnd, blur_mode_t mode)
    {
        switch (mode) {
        case blur_mode_t::DISABLE: return blur_disable(hwnd);
        case blur_mode_t::AERO:    return blur_aero(hwnd);
        case blur_mode_t::ACRYLIC: return blur_acrylic(hwnd);
        case blur_mode_t::MICA:    return blur_mica(hwnd);
        case blur_mode_t::MICAALT: return blur_mica(hwnd, true);
        default:
            if (probe::system::version() >= probe::WIN_11_22H2) {
                return blur_mica(hwnd, true);
            }
            if (probe::system::version() >= probe::WIN_11_21H2) {
                return blur_mica(hwnd, false);
            }
            if (probe::system::version() >= probe::WIN_10) {
                return blur_acrylic(hwnd);
            }
            return blur_aero(hwnd);
        }
    }

    HRESULT blur_disable(HWND hwnd)
    {
        const auto SetWindowCompositionAttribute =
            reinterpret_cast<pfnSetWindowCompositionAttribute>(probe::library::address_of(
                probe::library::load("user32.dll"), "SetWindowCompositionAttribute"));

        ACCENT_POLICY               accent = { ACCENT_DISABLED };
        WINDOWCOMPOSITIONATTRIBDATA blur{ WCA_ACCENT_POLICY, &accent, sizeof(ACCENT_POLICY) };
        SetWindowCompositionAttribute(hwnd, &blur);

        return ERROR_SUCCESS;
    }

    HRESULT blur_aero(HWND hwnd)
    {
        const auto SetWindowCompositionAttribute =
            reinterpret_cast<pfnSetWindowCompositionAttribute>(probe::library::address_of(
                probe::library::load("user32.dll"), "SetWindowCompositionAttribute"));

        ACCENT_POLICY               accent = { ACCENT_ENABLE_BLURBEHIND };
        WINDOWCOMPOSITIONATTRIBDATA blur{ WCA_ACCENT_POLICY, &accent, sizeof(ACCENT_POLICY) };
        SetWindowCompositionAttribute(hwnd, &blur);

        return ERROR_SUCCESS;
    }

    HRESULT blur_acrylic(HWND hwnd)
    {
        const auto SetWindowCompositionAttribute =
            reinterpret_cast<pfnSetWindowCompositionAttribute>(probe::library::address_of(
                probe::library::load("user32.dll"), "SetWindowCompositionAttribute"));

        ACCENT_POLICY accent = {
            .state     = ACCENT_ENABLE_ACRYLICBLURBEHIND,
            .flags     = 0x20 | 0x40 | 0x80 | 0x100,
            .color     = 0xBB000000,
            .animation = 0,
        };
        WINDOWCOMPOSITIONATTRIBDATA blur{ WCA_ACCENT_POLICY, &accent, sizeof(ACCENT_POLICY) };
        SetWindowCompositionAttribute(hwnd, &blur);

        // DWORD attr = DWMSBT_MAINWINDOW; // Mica 2 | MicaAlt 4
        //::DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &attr, sizeof(attr));
        return ERROR_SUCCESS;
    }

    HRESULT blur_mica(HWND hwnd, bool alt)
    {
        const auto SetWindowCompositionAttribute =
            reinterpret_cast<pfnSetWindowCompositionAttribute>(probe::library::address_of(
                probe::library::load("user32.dll"), "SetWindowCompositionAttribute"));

        ACCENT_POLICY               accent = { ACCENT_ENABLE_HOSTBACKDROP };
        WINDOWCOMPOSITIONATTRIBDATA blur{ WCA_ACCENT_POLICY, &accent, sizeof(ACCENT_POLICY) };
        SetWindowCompositionAttribute(hwnd, &blur);

        const DWORD mode = alt ? DWMSBT_TABBEDWINDOW : DWMSBT_MAINWINDOW;
        return ::DwmSetWindowAttribute(
            hwnd, (probe::system::version() >= probe::WIN_11_22H2) ? DWMWA_SYSTEMBACKDROP_TYPE : 1029,
            &mode, sizeof(mode));
    }

    // https://github.com/ysc3839/win32-darkmode/blob/master/win32-darkmode/DarkMode.h
    HRESULT update_theme(HWND hwnd, BOOL dark)
    {
        ::DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1, &dark, sizeof(dark));

        return ::DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    }
}; // namespace windows::dwm

void TransparentInput(QWidget *win, const bool en)
{
    const auto hwnd = reinterpret_cast<HWND>(win->winId());
    ::SetWindowLong(hwnd, GWL_EXSTYLE,
                    en ? GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_TRANSPARENT
                       : GetWindowLong(hwnd, GWL_EXSTYLE) & (~WS_EX_TRANSPARENT));
}

#endif