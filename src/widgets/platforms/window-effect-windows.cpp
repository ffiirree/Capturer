#include "window-effect.h"

#if _WIN32

#include <probe/library.h>
#include <probe/system.h>
#include <QWidget>
#include <winuser.h>

namespace windows::dwm
{
    HRESULT enable_shadow(HWND hwnd, bool en)
    {
        DWMNCRENDERINGPOLICY ncrp = en ? DWMNCRP_ENABLED : DWMNCRP_DISABLED;
        return ::DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &ncrp, sizeof(DWMNCRENDERINGPOLICY));
    }

    HRESULT set_window_corner(HWND hwnd, DWM_WINDOW_CORNER_PREFERENCE corner)
    {
        // This value is supported starting with Windows 11 Build 22000.
        return ::DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner,
                                       sizeof(DWMWINDOWATTRIBUTE));
    }

    HRESULT extended_frame_into_client(HWND hwnd, const MARGINS& margins)
    {
        return ::DwmExtendFrameIntoClientArea(hwnd, &margins);
    }

    HRESULT set_material(HWND hwnd, material_t material, bool enabled)
    {
        switch (material) {
        case material_t::Aero:    return set_material_aero(hwnd, enabled);
        case material_t::Acrylic: return set_material_acrylic(hwnd, 0xBB000000, enabled);
        case material_t::Mica:    return set_material_mica(hwnd, false, enabled);
        case material_t::MicaAlt: return set_material_mica(hwnd, true, enabled);
        default:
            if (probe::system::version() >= probe::WIN_11_22H2) {
                return set_material_mica(hwnd, true, enabled);
            }
            if (probe::system::version() >= probe::WIN_11_21H2) {
                return set_material_mica(hwnd, false, enabled);
            }
            if (probe::system::version() >= probe::WIN_10) {
                return set_material_acrylic(hwnd, 0xBB000000, enabled);
            }
            return set_material_aero(hwnd, enabled);
        }
    }

    HRESULT set_material_aero(HWND hwnd, bool enabled)
    {
        const auto SetWindowCompositionAttribute =
            reinterpret_cast<pfnSetWindowCompositionAttribute>(probe::library::address_of(
                probe::library::load("user32.dll"), "SetWindowCompositionAttribute"));

        ACCENT_POLICY accent = { static_cast<DWORD>(enabled ? ACCENT_ENABLE_BLURBEHIND : ACCENT_DISABLED) };
        WINDOWCOMPOSITIONATTRIBDATA blur{ WCA_ACCENT_POLICY, &accent, sizeof(ACCENT_POLICY) };
        if (SetWindowCompositionAttribute(hwnd, &blur) == FALSE) return E_FAIL;

        return S_OK;
    }

    // extended_frame_into_client(hwnd, { 0, 0, 0, 0 })
    HRESULT set_material_acrylic(HWND hwnd, DWORD color, bool enabled)
    {
        const auto SetWindowCompositionAttribute =
            reinterpret_cast<pfnSetWindowCompositionAttribute>(probe::library::address_of(
                probe::library::load("user32.dll"), "SetWindowCompositionAttribute"));

        ACCENT_POLICY accent = {};
        if (enabled) {
            accent = {
                .state = ACCENT_ENABLE_ACRYLICBLURBEHIND,
                .flags = ACCENT_ENABLE_BORDER,
                .color = color,
            };
        }
        WINDOWCOMPOSITIONATTRIBDATA blur{ WCA_ACCENT_POLICY, &accent, sizeof(ACCENT_POLICY) };
        if (SetWindowCompositionAttribute(hwnd, &blur) == FALSE) return E_FAIL;

        return S_OK;
    }

    // extended_frame_into_client(hwnd, { -1, -1, -1, -1 });
    HRESULT set_material_acrylic_v2(HWND hwnd, bool enabled)
    {
        const DWORD mode = enabled ? DWMSBT_TRANSIENTWINDOW : DWMSBT_AUTO;
        // 'DWMWA_SYSTEMBACKDROP_TYPE' is supported starting with Windows 11 Build 22621.
        return ::DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &mode, sizeof(mode));
    }

    // extended_frame_into_client(hwnd, { -1, -1, -1, -1 });
    HRESULT set_material_mica(HWND hwnd, bool alt, bool enabled)
    {
        if (probe::system::version() >= probe::WIN_11_22H2) {
            const DWORD mode = enabled ? (alt ? DWMSBT_TABBEDWINDOW : DWMSBT_MAINWINDOW) : DWMSBT_AUTO;
            // 'DWMWA_SYSTEMBACKDROP_TYPE' is supported starting with Windows 11 Build 22621.
            return ::DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &mode, sizeof(mode));
        }

        // >= Windows 11 21H2
        const BOOL enable = enabled ? TRUE : FALSE;
        return ::DwmSetWindowAttribute(hwnd, 1029, &enable, sizeof(enable));
    }

    // https://github.com/ysc3839/win32-darkmode/blob/master/win32-darkmode/DarkMode.h
    HRESULT set_dark_mode(HWND hwnd, BOOL dark)
    {
        if (probe::system::version() >= probe::WIN_11_21H2) {
            return ::DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        }

        return ::DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
    }
} // namespace windows::dwm

void TransparentInput(QWidget *win, const bool en)
{
    const auto hwnd = reinterpret_cast<HWND>(win->winId());
    ::SetWindowLong(hwnd, GWL_EXSTYLE,
                    en ? GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_TRANSPARENT
                       : GetWindowLong(hwnd, GWL_EXSTYLE) & (~WS_EX_TRANSPARENT));
}

#endif