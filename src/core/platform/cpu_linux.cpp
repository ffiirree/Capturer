#ifdef __linux__

#include "platform.h"
#include <sys/utsname.h>
#include <cstring>
#include <fstream>
#include <string>
#include <algorithm>
#include <unordered_set>

namespace platform::cpu
{
    static std::optional<std::string> cpuinfo_read_first_of(const char * key)
    {
        std::ifstream cpuinfo("/proc/cpuinfo");

        if(!cpuinfo.is_open() || !cpuinfo)
            return std::nullopt;

        for(std::string line; std::getline(cpuinfo, line);) {
            if(line.find(key) == 0) {
                const auto nonspace_id = line.find_first_not_of(" \t", line.find_first_of(':') + 1);
                return line.c_str() + nonspace_id;
            }
        }

        return std::nullopt;
    }

    static uint32_t cpuinfo_count_of(const char * key)
    {
        std::ifstream cpuinfo("/proc/cpuinfo");

        if(!cpuinfo.is_open() || !cpuinfo)
            return 0;

        uint32_t counter = 0;
        for(std::string line; std::getline(cpuinfo, line);) {
            if(line.find(key) == 0) {
                counter++;
            }
        }
        return counter;
    }

    static uint32_t cpuinfo_unique_count_of(const char * key)
    {
        std::ifstream cpuinfo("/proc/cpuinfo");

        if(!cpuinfo.is_open() || !cpuinfo)
            return 0;

        std::unordered_set<std::string> values{};
        for(std::string line; std::getline(cpuinfo, line);) {
            if(line.find(key) == 0) {
                const auto nonspace_id = line.find_first_not_of(" \t", line.find_first_of(':') + 1);
                values.insert(line.c_str() + nonspace_id);
            }
        }
        return values.size();
    }

    architecture_t architecture()
    {
        utsname buf{};
        if(uname(&buf) == -1)
            return cpu::architecture_t::unknown;

        if(!strcmp(buf.machine, "x86_64"))
            return cpu::architecture_t::x86_64;
        else if(strstr(buf.machine, "arm") == buf.machine)
            return cpu::architecture_t::arm;
        else if(!strcmp(buf.machine, "ia64") || !strcmp(buf.machine, "IA64"))
            return cpu::architecture_t::itanium;
        else if(!strcmp(buf.machine, "i686"))
            return cpu::architecture_t::x86;
        else
            return cpu::architecture_t::unknown;
    }

    uint64_t frequency()
    {
        return static_cast<uint64_t>(std::strtod(
                cpuinfo_read_first_of("cpu MHz").value_or("0").c_str(),
                nullptr
        ) * 1'000'000);
    }

    // https://stackoverflow.com/questions/150355/programmatically-find-the-number-of-cores-on-a-machine
    quantities_t quantities()
    {
        cpu::quantities_t ret{};

        ret.logical = cpuinfo_count_of("processor");
        ret.packages = cpuinfo_unique_count_of("physical id");
        ret.physical = ret.logical / ret.packages;

        return ret;
    }

    std::string vendor()
    {
        return cpuinfo_read_first_of("vendor").value_or("");
    }

    std::string name()
    {
        return cpuinfo_read_first_of("model name").value_or("");
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

#endif // __linux__