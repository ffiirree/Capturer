#include "platform.h"
#include <limits>

namespace platform
{
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

    std::string system::desktop_name(system::desktop_t de)
    {
        switch (de)
        {
        case system::desktop_t::Windows:    return "Windows";
        case system::desktop_t::KDE:        return "KDE";
        case system::desktop_t::GNOME:      return "GNOME";
        case system::desktop_t::Unity:      return "Unity";
        case system::desktop_t::MATE:       return "MATE";
        case system::desktop_t::Cinnamon:   return "Cinnamon";
        case system::desktop_t::Xfce:       return "Xfce";
        case system::desktop_t::DeepinDE:   return "DeepinDE";
        case system::desktop_t::Enlightenment: return "Enlightenment";
        case system::desktop_t::LXQT:       return "LXQT";
        case system::desktop_t::Lumina:     return "Lumina";

        case system::desktop_t::unknown:
        default:                            return "unknown";
        }
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

    bool display::geometry_t::contains(int32_t _x, int32_t _y) const
    {
        return (_x >= left() && _x < right()) && (_y >= top() && _y < bottom());
    }

    display::geometry_t display::geometry_t::intersected(const display::geometry_t& otr) const
    {
        int32_t l = std::max(left(), otr.left());
        int32_t t = std::max(top(), otr.top());

        int32_t r = std::min(right(), otr.right());
        int32_t b = std::min(bottom(), otr.bottom());

        if(r <= l || b <= t) {
            return {};
        }

        return {l, t, static_cast<uint32_t>(r - l + 1), static_cast<uint32_t>(b - t + 1) };
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

    display::display_t display::virtual_screen()
    {
        return { "~VIRTUAL-SCREEN", virtual_screen_geometry(), 60.0, 32 };
    }

    namespace util 
    {
        std::string to_utf8(const std::wstring& wstr)
        {
            return to_utf8(wstr.c_str(), wstr.size());
        }

        std::string to_utf8(const char* wstr, size_t size)
        {
            return to_utf8(reinterpret_cast<const wchar_t*>(wstr), size);
        }

        std::string to_utf8(const std::string& wstr)
        {
            // the length of wstr is not known, use null to detect the real length
            return to_utf8(reinterpret_cast<const wchar_t *>(wstr.c_str()), 0);
        }

        std::wstring to_utf16(const std::string& mstr)
        {
            return to_utf16(mstr.c_str(), mstr.size());
        }
    }
} // namespace platform
