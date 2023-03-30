#include "platform.h"
#include <limits>

#ifdef  _WIN32
#include <windows.h>
#endif

#ifdef _WIN32

namespace platform::windows {
    // https://learn.microsoft.com/en-us/windows/win32/sysinfo/registry-value-types
    // REG_DWORD        : A 32-bit number.
    // REG_QWORD        : A 64-bit number.
    std::optional<DWORD> reg_read_dword(HKEY key, const char* subkey, const char* valuename)
    {
        DWORD value = 0;
        DWORD size = sizeof(uint32_t);
        if (RegGetValueA(key, subkey, valuename, RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS) {
            return value;
        }

        return std::nullopt;
    }

    // REG_SZ	        : A null - terminated string. 
    //                    It's either a Unicode or an ANSI string, 
    //                    depending on whether you use the Unicode or ANSI functions.
    std::optional<std::string> reg_read_string(HKEY key, const char* subkey, const char* valuename)
    {

        DWORD size = 0;
        if (RegGetValueA(key, subkey, valuename, RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS) {
            return std::nullopt;
        }

        std::string value(size, {});

        if (RegGetValueA(key, subkey, valuename, RRF_RT_REG_SZ, nullptr, reinterpret_cast<LPBYTE>(&value[0]), &size) != ERROR_SUCCESS) {
            return std::nullopt;
        }

        return value;
    }
}

#endif //  _WIN32

platform::cpu::endianness_t platform::cpu::endianness()
{
    const uint16_t test = 0xFF00;
    const auto result = *static_cast<const std::uint8_t*>(static_cast<const void*>(&test));

    if (result == 0xFF)
        return endianness_t::big;
    else
        return endianness_t::little;
}

platform::display::geometry_t platform::display::virtual_screen_geometry()
{
    int32_t l = std::numeric_limits<int32_t>::max(), t = std::numeric_limits<int32_t>::max();
    int32_t r = std::numeric_limits<int32_t>::min(), b = std::numeric_limits<int32_t>::min();

    for (auto& display : displays()) {
        if (l > display.geometry.x) l = display.geometry.x;

        if (t > display.geometry.y) t = display.geometry.y;

        int32_t this_r = display.geometry.x + display.geometry.width - 1;
        if (r < this_r) r = this_r;

        int32_t this_b = display.geometry.y + display.geometry.height - 1;
        if (b < this_b) b = this_b;
    }

    return platform::display::geometry_t{ l, t, static_cast<uint32_t>(r - l + 1), static_cast<uint32_t>(b - t + 1) };
}