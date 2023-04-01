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
} // namespace platform::windows

#elif __linux__

namespace platform::linux {
    std::optional<std::string> exec(const char * cmd)
    {
        char buffer[128];
        std::string result{};

        FILE* pipe = popen(cmd, "r");

        if (!pipe) return std::nullopt;

        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }

        pclose(pipe);
        return result;
    }    
} // namespace platform::linux

#endif //  _WIN32

namespace platform {

    static uint64_t numeric_combined(uint32_t h, uint32_t l) { return static_cast<uint64_t>(h) << 32 | l; }

    bool version_t::operator >= (const version_t& r) const
    {
        if (numeric_combined(major, minor) > numeric_combined(r.major, r.minor)) return true;

        if (numeric_combined(major, minor) < numeric_combined(r.major, r.minor)) return false;

        return numeric_combined(patch, build) >= numeric_combined(r.patch, r.build);
    }

    bool version_t::operator <= (const version_t& r) const
    {
        if (numeric_combined(major, minor) > numeric_combined(r.major, r.minor)) return false;

        if (numeric_combined(major, minor) < numeric_combined(r.major, r.minor)) return true;

        return numeric_combined(patch, build) <= numeric_combined(r.patch, r.build);
    }

    template<> vendor_t vendor_cast(uint32_t id)
    {
        switch (id)
        {
        case static_cast<uint32_t>(vendor_t::NVIDIA):
        case static_cast<uint32_t>(vendor_t::Intel):
        case static_cast<uint32_t>(vendor_t::Microsoft):
        case static_cast<uint32_t>(vendor_t::Qualcomm):
        case static_cast<uint32_t>(vendor_t::AMD):
        case static_cast<uint32_t>(vendor_t::Apple):
            return static_cast<vendor_t>(id);

        default: return vendor_t::unknown;
        }
    }

    template <> std::string vendor_cast(vendor_t vendor)
    {
        switch (vendor)
        {
        case vendor_t::NVIDIA:      return "NVIDIA Corporation";
        case vendor_t::Intel:       return "Intel Corporation";
        case vendor_t::Microsoft:   return "Microsoft Corporation";
        case vendor_t::Qualcomm:    return "Qualcomm Technologies";
        case vendor_t::AMD:         return "Advanced Micro Devices, Inc.";
        case vendor_t::Apple:       return "Apple Inc.";
        case vendor_t::unknown:
        default:                    return "unknown";
        }
    }

    template <> vendor_t vendor_cast(std::string_view name)
    {
        if (name.find("NVIDIA") != std::string::npos) return vendor_t::NVIDIA;
        if (name.find("Intel") != std::string::npos) return vendor_t::Intel;
        if (name.find("Microsoft") != std::string::npos) return vendor_t::Microsoft;
        if (name.find("Qualcomm") != std::string::npos) return vendor_t::Qualcomm;
        if (name.find("AMD") != std::string::npos ||
            name.find("Advanced Micro Devices") != std::string::npos) return vendor_t::AMD;
        if (name.find("Apple") != std::string::npos) return vendor_t::Apple;
        
        return vendor_t::unknown;
    }

    std::string system::theme_name(system::theme_t t)
    {
        return (t == system::theme_t::dark) ? "dark" : "light";
    }

    cpu::endianness_t cpu::endianness()
    {
        const uint16_t test = 0xFF00;
        const auto result = *static_cast<const std::uint8_t*>(static_cast<const void*>(&test));

        if (result == 0xFF)
            return endianness_t::big;
        else
            return endianness_t::little;
    }

    display::geometry_t display::virtual_screen_geometry()
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

        return display::geometry_t{
            l, t,
            static_cast<uint32_t>(r - l + 1), static_cast<uint32_t>(b - t + 1)
        };
    }
} // namespace platform
