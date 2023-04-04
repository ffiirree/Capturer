#ifdef _WIN32

#include "platform.h"
#include "defer.h"
#include <utility>
#include <ShellScalingApi.h>
#include "logging.h"

namespace platform::display {
    // https://learn.microsoft.com/en-us/windows/win32/hidpi/setting-the-default-dpi-awareness-for-a-process
    // <  Windows 8.1       : GetDeviceCaps(hdc, LOGPIXELSX)
    // >= Windows 8.1       : GetDpiForMonitor(, MDT_EFFECTIVE_DPI, ,) with SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE), but can lead to unexpected application behavior.
    // >= Windows 10, 1607  : GetDpiForMonitor(, MDT_EFFECTIVE_DPI, ,) with SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)
    //
    // Just for >= 1607
    static UINT retrieve_dpi_for_monitor(HMONITOR monitor)
    {
        UINT dpi = 96, _;      // default value without scaling

        const auto PRE_DAC = ::SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        ::GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi, &_);
        ::SetThreadDpiAwarenessContext(PRE_DAC);

        return dpi;
    }

    // https://learn.microsoft.com/en-us/windows/win32/gdi/multiple-display-monitors
    //
    // The bounding rectangle of all the monitors is the `virtual screen`.
    // The `desktop` covers the virtual screen instead of a single monitor. 
    // The `primary monitor` contains the origin (0,0) for compatibility.
    //
    // Each `physical display` is represented by a monitor handle of type HMONITOR. 
    // A valid HMONITOR is guaranteed to be non-NULL.
    // A physical display has the same HMONITOR as long as it is part of the desktop. 
    // When a WM_DISPLAYCHANGE message is sent, any monitor may be removed from the desktop 
    // and thus its HMONITOR becomes invalid or has its settings changed. 
    // Therefore, an application should check whether all HMONITORS are valid when this message is sent.
    //
    // Any function that returns a display device context(DC) normally returns a DC for the primary monitor.
    // To obtain the DC for another monitor, use the EnumDisplayMonitors function.
    // Or, you can use the device name from the GetMonitorInfo function to create a DC with CreateDC.
    // However, if the function, such as GetWindowDC or BeginPaint, gets a DC for a window that spans more than one display, 
    // the DC will also span the two displays.
    //
    // To enumerate all the devices on the computer, call the EnumDisplayDevices function. 
    // The information that is returned also indicates which monitor is part of the desktop.
    //
    // To enumerate the devices on the desktop that intersect a clipping region, call EnumDisplayMonitors. 
    // This returns the HMONITOR handle to each monitor, which is used with GetMonitorInfo. 
    // To enumerate all the devices in the virtual screen, use EnumDisplayMonitors.
    //
    // When using multiple monitors as independent displays, the desktop contains one display or set of displays.
    std::vector<display_t> displays()
    {
        std::vector<display_t> ret{};

        // prevent the GetMonitorInfoA from being affected by the process dpi awareness 
        auto PRE_DAC = ::SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_UNAWARE);

        // retrieve all monitors
        ::EnumDisplayMonitors(nullptr, nullptr, [](auto monitor, auto, auto, auto userdata)->BOOL {
            auto ret = reinterpret_cast<std::vector<display_t> *>(userdata);

            MONITORINFOEXA info = { sizeof(MONITORINFOEXA) };
            if (::GetMonitorInfoA(monitor, &info)) {

                DEVMODEA settings = {};
                settings.dmSize = sizeof(DEVMODEA);

                if (::EnumDisplaySettingsA(info.szDevice, ENUM_CURRENT_SETTINGS, &settings)) {

                    ret->push_back(display_t{
                        info.szDevice,
                        geometry_t{
                            settings.dmPosition.x,
                            settings.dmPosition.y,
                            settings.dmPelsWidth,
                            settings.dmPelsHeight,
                        },
                        static_cast<double>(settings.dmDisplayFrequency),
                        settings.dmBitsPerPel,
                        retrieve_dpi_for_monitor(monitor),
                        static_cast<orientation_t>(0x01 << settings.dmDisplayOrientation),
                        (settings.dmPosition.x == 0) && (settings.dmPosition.y == 0),
                        static_cast<float>(settings.dmPelsWidth) / (info.rcMonitor.right - info.rcMonitor.left)
                    });
                }
            }

            return TRUE;
        }, reinterpret_cast<LPARAM>(&ret));

        ::SetThreadDpiAwarenessContext(PRE_DAC);

        return ret;
    }
}

#endif // _WIN32
