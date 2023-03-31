#ifdef _WIN32

#include "platform.h"
#include "defer.h"
#include <utility >
#include "logging.h"

namespace platform::display {
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
        std::vector<display_t> _displays{};

        DWORD           adapter_idx = 0;        // display adapter
        DISPLAY_DEVICEA device = {};
        CHAR            adapter_name[33];  // 32 + 1 for the null-terminator 

        device.cb = sizeof(DISPLAY_DEVICEA);

        // Display Adapter
        while (EnumDisplayDevicesA(NULL, adapter_idx, &device, 0))
        {
            memset(adapter_name, 0, sizeof(adapter_name));
            memcpy(adapter_name, device.DeviceName, sizeof(device.DeviceName));
            memset(device.DeviceName, 0, sizeof(device.DeviceName));

            // Display Monitor
            int monitor_idx = 0;
            while (EnumDisplayDevicesA(adapter_name, monitor_idx, &device, 0))
            {
                if (device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) {
                    DEVMODEA settings{};
                    settings.dmSize = sizeof(DEVMODEA);
                    if (!EnumDisplaySettingsA(adapter_name, ENUM_CURRENT_SETTINGS, &settings))
                        LOG(ERROR) << "Store default failed";

                    _displays.push_back({
                        adapter_name,
                        device.DeviceID,
                        geometry_t{
                            settings.dmPosition.x,
                            settings.dmPosition.y,
                            settings.dmPelsWidth,
                            settings.dmPelsHeight,
                        },
                        static_cast<double>(settings.dmDisplayFrequency),
                        settings.dmBitsPerPel,
                        96,             // TODO: 
                        static_cast<orientation_t>(0x01 << settings.dmDisplayOrientation)
                    });

                    ++monitor_idx;
                }
            }

            ++adapter_idx;
        } // end while for all display devices

        return _displays;
    }
}

#endif // _WIN32
