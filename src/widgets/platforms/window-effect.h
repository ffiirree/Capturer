#ifndef CAPTURER_WINDOW_EFFECT_H
#define CAPTURER_WINDOW_EFFECT_H

#if _WIN32

#include <dwmapi.h>
#include <probe/library.h>

enum WINDOWCOMPOSITIONATTRIB : DWORD
{
    WCA_UNDEFINED                     = 0,
    WCA_NCRENDERING_ENABLED           = 1,
    WCA_NCRENDERING_POLICY            = 2,
    WCA_TRANSITIONS_FORCEDISABLED     = 3,
    WCA_ALLOW_NCPAINT                 = 4,
    WCA_CAPTION_BUTTON_BOUNDS         = 5,
    WCA_NONCLIENT_RTL_LAYOUT          = 6,
    WCA_FORCE_ICONIC_REPRESENTATION   = 7,
    WCA_EXTENDED_FRAME_BOUNDS         = 8,
    WCA_HAS_ICONIC_BITMAP             = 9,
    WCA_THEME_ATTRIBUTES              = 10,
    WCA_NCRENDERING_EXILED            = 11,
    WCA_NCADORNMENTINFO               = 12,
    WCA_EXCLUDED_FROM_LIVEPREVIEW     = 13,
    WCA_VIDEO_OVERLAY_ACTIVE          = 14,
    WCA_FORCE_ACTIVEWINDOW_APPEARANCE = 15,
    WCA_DISALLOW_PEEK                 = 16,
    WCA_CLOAK                         = 17,
    WCA_CLOAKED                       = 18,
    WCA_ACCENT_POLICY                 = 19,
    WCA_FREEZE_REPRESENTATION         = 20,
    WCA_EVER_UNCLOAKED                = 21,
    WCA_VISUAL_OWNER                  = 22,
    WCA_HOLOGRAPHIC                   = 23,
    WCA_EXCLUDED_FROM_DDA             = 24,
    WCA_PASSIVEUPDATEMODE             = 25,
    WCA_USEDARKMODECOLORS             = 26,
    WCA_CORNER_STYLE                  = 27,
    WCA_PART_COLOR                    = 28,
    WCA_DISABLE_MOVESIZE_FEEDBACK     = 29,
    WCA_LAST                          = 30
};

struct WINDOWCOMPOSITIONATTRIBDATA
{
    WINDOWCOMPOSITIONATTRIB Attrib;
    PVOID                   pvData;
    SIZE_T                  cbData;
};

enum ACCENT_STATE
{
    ACCENT_DISABLED                   = 0,
    ACCENT_ENABLE_GRADIENT            = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND          = 3, // Aero effect
    ACCENT_ENABLE_ACRYLICBLURBEHIND   = 4, // Acrylic effect
    ACCENT_ENABLE_HOSTBACKDROP        = 5, // Mica effect
    ACCENT_INVALID_STATE              = 6  // Using this value will remove the window background
};

enum ACCENT_FLAG
{
    ACCENT_NONE                           = 0,
    ACCENT_ENABLE_ACRYLIC                 = 1,
    ACCENT_ENABLE_ACRYLIC_WITH_LUMINOSITY = 482
};

struct ACCENT_POLICY
{
    DWORD state;
    DWORD flags;
    DWORD color; // #AABBGGRR
    DWORD animation;
};

typedef BOOL(WINAPI *pfnSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA *);

namespace windows::dwm
{
    enum class blur_mode_t : unsigned char
    {
        DISABLE,
        DEFAULT,
        AERO,
        ACRYLIC,
        MICA,
        MICAALT
    };

    HRESULT set_window_corner(HWND hwnd, DWM_WINDOW_CORNER_PREFERENCE corner);

    HRESULT enable_shadow(HWND hwnd, bool en);

    HRESULT blur_behind(HWND hwnd, BOOL en = true);

    HRESULT blur(HWND hwnd, blur_mode_t mode);

    HRESULT blur_disable(HWND hwnd);
    HRESULT blur_aero(HWND hwnd);
    HRESULT blur_acrylic(HWND hwnd);
    HRESULT blur_mica(HWND hwnd, bool alt = false);

    HRESULT update_theme(HWND hwnd, BOOL dark = FALSE);
}; // namespace windows::dwm

#elif __linux__

namespace x11
{

} // namespace x11

#endif

class QWidget;

void TransparentInput(QWidget *win, bool en);

#endif //! CAPTURER_WINDOW_EFFECT_H