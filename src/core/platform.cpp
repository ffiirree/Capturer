#include "platform.h"

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
