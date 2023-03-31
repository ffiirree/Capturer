#ifndef PLATFORM_H
#define PLATFORM_H

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

#ifdef _WIN32

#include <Windows.h>

#endif

namespace platform {

    struct version_t {
        uint32_t major;
        uint32_t minor;
        uint32_t patch;
        uint32_t build;

        std::string codename;
    };

#ifdef _WIN32
    namespace windows {
        std::optional<DWORD> reg_read_dword(HKEY key, const char*, const char*);
        std::optional<std::string> reg_read_string(HKEY key, const char*, const char*);
    }
#elif __linux__
    namespace linux {
        std::optional<std::string> exec(const char *);
    }
#endif // _WIN32

    namespace system {

        enum class theme_t
        {
            unknown, dark, light, custom
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

        std::string os_name();
        std::string kernel_name();

        version_t os_version();
        version_t kernel_version();

        os_info_t os_info();
        kernel_info_t kernel_info();
    }

    namespace cpu {
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

    namespace gpu {
        struct gpu_info_t
        {
            std::string name;
            std::string vendor;

            size_t memory_size;
            size_t frequency;
        };

        gpu_info_t info();
    }

    namespace display {
        struct geometry_t
        {
            int32_t x;
            int32_t y;
            uint32_t width;
            uint32_t height;
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
            std::string id;

            geometry_t geometry;
            double frequency;           // Hz

            uint32_t bpp;
            uint32_t dpi{ 96 };         // logical dot per inch
            orientation_t orientation{ orientation_t::landscape };
            bool primary{ false };
        };

        std::vector<display_t> displays();

        geometry_t virtual_screen_geometry();
    }
}

#endif // !PLATFORM_H
