#ifdef _WIN32

#include "platform.h"
#include <Windows.h>
#include <vector>
#include <bitset>

namespace platform::cpu 
{
    architecture_t architecture()
    {
        SYSTEM_INFO system_info;
        GetNativeSystemInfo(&system_info);

        switch (system_info.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: // x64 (AMD or Intel)
            return architecture_t::x64;

        case PROCESSOR_ARCHITECTURE_ARM: // ARM
            return architecture_t::arm;

        case PROCESSOR_ARCHITECTURE_ARM64: // ARM64
            return architecture_t::arm64;

        case PROCESSOR_ARCHITECTURE_IA64: // Intel Itanium-based
            return architecture_t::ia64;

        case PROCESSOR_ARCHITECTURE_INTEL: // x86
            return architecture_t::x86;

        default:
            return architecture_t::unknown;
        }
    }

    uint64_t frequency()
    {
        return platform::windows::reg_read_dword(
            HKEY_LOCAL_MACHINE,
            R"(HARDWARE\DESCRIPTION\System\CentralProcessor\0)",
            "~MHz"
        ).value_or(0) * 1'000'000;
    }

    quantities_t quantities()
    {
        std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> processors;

        DWORD byte_count = 0;
        GetLogicalProcessorInformation(nullptr, &byte_count);

        processors.resize(byte_count / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
        GetLogicalProcessorInformation(processors.data(), &byte_count);

        quantities_t q = { 0 };
        for (auto&& info : processors) {
            switch (info.Relationship)
            {
            case RelationProcessorCore:
                q.physical++;
                q.logical += static_cast<std::uint32_t>(std::bitset<sizeof(ULONG_PTR) * 8>(static_cast<uintptr_t>(info.ProcessorMask)).count());
                break;

            case RelationProcessorPackage:
                q.packages++;
                break;

            default:
                break;
            }
        }

        return q;
    }

    std::string vendor()
    {
        return platform::windows::reg_read_string(
            HKEY_LOCAL_MACHINE, 
            R"(HARDWARE\DESCRIPTION\System\CentralProcessor\0)", 
            "VendorIdentifier"
        ).value_or("");
    }

    std::string name()
    {
        return platform::windows::reg_read_string(
            HKEY_LOCAL_MACHINE, 
            R"(HARDWARE\DESCRIPTION\System\CentralProcessor\0)",
            "ProcessorNameString"
        ).value_or("");
    }

    cpu_info_t info()
    {
        return {
            name(),
            vendor(),
            architecture(),
            endianness(),
            frequency(),
            quantities()
        };
    }
}

#endif // _WIN32