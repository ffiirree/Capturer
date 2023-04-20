#ifndef PLATFORM_H
#define PLATFORM_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <deque>
#include <optional>
#include <QRect>
#include <functional>
#include <thread>
#include <atomic>

#ifdef _WIN32

#include <Windows.h>

#endif

namespace platform 
{
    struct version_t 
    {
        uint32_t major{};
        uint32_t minor{};
        uint32_t patch{};
        uint32_t build{};

        std::string codename{};

        bool operator >= (const version_t&) const;
        bool operator <= (const version_t&) const;
        bool operator == (const version_t&) const;
    };

#ifdef _WIN32
    namespace windows 
    {
        // https://en.wikipedia.org/wiki/List_of_Microsoft_Windows_versions
        // windows 3.1, 1992-04-06
        inline const version_t WIN_3_1_1ST{ 3, 10, 102, 0, "Sparta" };

        // windows 95, 1995-08-24
        inline const version_t WIN_95_1ST{ 4, 0, 950, 0, "Chicago" };

        // windows 98, 1998-06-25
        inline const version_t WIN_98_1ST{ 4, 10, 1998, 0, "Memphis" };

        // windows 2000
        inline const version_t WIN_2000_1ST{ 5, 0, 2195, 0, "Janus" };

        // windows xp
        inline const version_t WIN_XP_1ST{ 5, 2, 2600, 0, "Whistler" };

        // windows vista
        inline const version_t WIN_VISTA_1ST{ 6, 0, 6000, 0, "Longhorn" };

        // windows 7
        inline const version_t WIN_7_1ST{ 6, 1, 7600, 0, "7" };

        // windows 8
        inline const version_t WIN_8_0_1ST{ 6, 2, 9200, 0, "8" };
        inline const version_t WIN_8_1_1ST{ 6, 3, 9600, 0, "Blue"};

        // windows 10
        inline const version_t WIN_10_1ST{ 10, 0, 10240, 16405, "1507" };
        inline const version_t WIN_10_1507{ 10, 0, 10240, 16405, "1507" };
        inline const version_t WIN_10_1511{ 10, 0, 10586, 3, "1511" };
        inline const version_t WIN_10_1607{ 10, 0, 14393, 10, "1607" };
        inline const version_t WIN_10_1703{ 10, 0, 15063, 138, "1703" };
        inline const version_t WIN_10_1709{ 10, 0, 16299, 19, "1709" };
        inline const version_t WIN_10_1803{ 10, 0, 17134, 48, "1803" };
        inline const version_t WIN_10_1809{ 10, 0, 17763, 1, "1809" };
        inline const version_t WIN_10_1903{ 10, 0, 18362, 116, "1903" };
        inline const version_t WIN_10_1909{ 10, 0, 18363, 476, "1909" };
        inline const version_t WIN_10_2004{ 10, 0, 19041, 264, "2004" };
        inline const version_t WIN_10_20H2{ 10, 0, 19042, 572, "20H2" };
        inline const version_t WIN_10_21H1{ 10, 0, 19043, 985, "21H1" };
        inline const version_t WIN_10_21H2{ 10, 0, 19044, 288, "21H2" };
        inline const version_t WIN_10_22H2{ 10, 0, 19045, 2130, "22H2" };

        // windows 11
        inline const version_t WIN_11_1ST{ 10, 0, 22000, 194, "21H2" };
        inline const version_t WIN_11_21H2{ 10, 0, 22000, 194, "21H2" };
        inline const version_t WIN_11_22H2{ 10, 0, 22621, 521, "22H2" };

        std::optional<DWORD> reg_read_dword(HKEY key, const std::string&, const std::string&);
        std::optional<std::string> reg_read_string(HKEY key, const std::string&, const std::string&);

        class RegistryMonitor {
        public:
            RegistryMonitor() = default;
            ~RegistryMonitor() { stop(); }

            RegistryMonitor(const RegistryMonitor&) = delete;
            RegistryMonitor& operator= (const RegistryMonitor&) = delete;

            int monitor(HKEY key, const std::string&, const std::function<void(HKEY)>&);
            void stop();

        private:
            HKEY key_;
            HANDLE STOP_EVENT{ nullptr };
            HANDLE NOTIFY_EVENT{ nullptr };
            std::thread thread_;
            std::atomic<bool> running_{ false };
        };

        std::shared_ptr<RegistryMonitor> monitor_regkey(HKEY key, const std::string& subkey, std::function<void(HKEY)> cb);
    }
#elif __linux__
    namespace linux 
    {
        std::optional<std::string> exec(const char *);

        class GSettingsMonitor {
        public:
            GSettingsMonitor() = default;
            ~GSettingsMonitor() { stop(); }

            GSettingsMonitor(const GSettingsMonitor&) = delete;
            GSettingsMonitor& operator= (const GSettingsMonitor&) = delete;

            int monitor(const std::string&, const std::string&, std::function<void(const std::string&)>);
            void stop();

        private:
            std::thread thread_;
            std::atomic<bool> running_{ false };
        };

        std::shared_ptr<GSettingsMonitor> monitor_gsettings(const std::string&, const std::string&, std::function<void(const std::string&)>);
    }
#endif // _WIN32

    enum class vendor_t
    {
        unknown = 0x00,
        NVIDIA = 0x10de,
        Intel = 0x8086,
        Microsoft = 0x1414,
        Qualcomm = 0x17cb,
        AMD = 0x1002,
        Apple = 0x106b,
    };

    template <class O, class I> O vendor_cast(I i) { return static_cast<O>(i); }

    template <> vendor_t vendor_cast(uint32_t);
    template <> std::string vendor_cast(vendor_t);
    template <> vendor_t vendor_cast(std::string_view name);

    namespace system 
    {
        enum class desktop_t
        {
            unknown,
            Windows,    // Windows
            KDE,        // K Desktop Environment, based on Qt
            GNOME,      // GNU Network Object Model Environment
            Unity,      // based on GNOME
            MATE,       // forked from GNOME 2
            Cinnamon,   // forked from GNOME 3
            Xfce,
            DeepinDE,   // based on Qt
            Enlightenment,
            LXQT,
            Lumina
        };

        enum class theme_t
        {
            dark, light
        };

        struct os_info_t
        {
            std::string name;
            theme_t theme;

            version_t version;
        };

        struct kernel_info_t
        {
            std::string name;
            version_t version;
        };

        theme_t theme();
        std::string theme_name(theme_t);

        desktop_t desktop();
        std::string desktop_name(desktop_t);

        std::string os_name();
        std::string kernel_name();

        version_t os_version();
        version_t kernel_version();

        os_info_t os_info();
        kernel_info_t kernel_info();
    }

    namespace cpu 
    {
        enum class endianness_t
        {
            little, big
        };

        enum class architecture_t
        {
            unknown,
            x86, i386 = x86,
            ia64, itanium = ia64,
            x64, amd64 = x64, x86_64 = x64,
            arm,
            arm64
        };

        struct quantities_t {
            /// Hyperthreads.
            std::uint32_t logical;
            /// Physical "cores".
            std::uint32_t physical;
            /// Physical CPU units/packages/sockets.
            std::uint32_t packages;
        };

        struct cpu_info_t
        {
            std::string name;
            std::string vendor;
            architecture_t arch;

            endianness_t endianness;
            uint64_t frequency;
            quantities_t quantities;
        };

        architecture_t architecture();

        endianness_t endianness();

        quantities_t quantities();

        uint64_t frequency();

        std::string vendor();
        std::string name();

        cpu_info_t info();
    }

    namespace gpu 
    {
        struct gpu_info_t
        {
#ifdef _WIN32
            std::wstring name;
#elif __linux__
            std::string name;
#endif // _WIN32

            vendor_t vendor;

            size_t dedicated_memory;   // B
            size_t shared_memory;      // B
            size_t frequency;
        };

        std::vector<gpu_info_t> info();
    }

    namespace display 
    {
        struct geometry_t
        {
            int32_t x;
            int32_t y;
            uint32_t width;
            uint32_t height;

            [[nodiscard]] bool contains(int32_t, int32_t) const;
            [[nodiscard]] geometry_t intersected(const geometry_t&) const;

            [[nodiscard]] int32_t left() const { return x; }
            [[nodiscard]] int32_t top() const { return y; }
            [[nodiscard]] int32_t right() const { return x + static_cast<int32_t>(width) - 1; }
            [[nodiscard]] int32_t bottom() const { return y + static_cast<int32_t>(height) - 1; }

            operator QRect() const { return QRect{ x, y, static_cast<int>(width), static_cast<int>(height) }; }
        };

        struct window_t
        {
            std::string name;           // utf-8
            std::string classname;

            geometry_t rect;
            uint64_t handle;
            bool visible;
        };

        enum class orientation_t
        {
            landscape = 0x01,
            portrait = 0x02,

            rotate_0 = landscape, rotate_90 = portrait, rotate_180 = 0x04, rotate_270=0x08,
#ifdef _WIN32
            landscape_flipped = rotate_180,
            portrait_flipped = rotate_270
#elif __linux__
            flipped = 0x10,

            landscape_flipped = flipped,
            portrait_flipped = rotate_90 | flipped
#endif
        };

        struct display_t
        {
            std::string name;

            geometry_t geometry;
            double frequency;           // Hz

            uint32_t bpp;
            uint32_t dpi{ 96 };         // logical dot per inch
            orientation_t orientation{ orientation_t::landscape };
            bool primary{ false };
            double scale { 1.0 };
        };

        std::vector<display_t> displays();

        std::deque<window_t> windows(bool=true);

        display_t virtual_screen();

        geometry_t virtual_screen_geometry();
    }

    // string
    namespace util 
    {
        std::string to_utf8(const wchar_t*, size_t = 0);
        std::string to_utf8(const std::wstring&);

        std::string to_utf8(const char*, size_t = 0); // must has a null-terminating-character if the size_t == 0
        std::string to_utf8(const std::string&);

        std::wstring to_utf16(const std::string&);
        std::wstring to_utf16(const char*, size_t = 0);

        // linux: The thread name is a meaningful C language string, whose length is restricted to 16 characters,
        //        including the terminating null byte ('\0')
        int thread_set_name(const std::string&);
        std::string thread_get_name();

        // TODO: wrap the std::thread with name
    }
}

#endif // !PLATFORM_H
