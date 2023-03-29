#ifdef __linux__

#include "platform.h"
#include <sys/utsname.h>
#include <fstream>
#include <unordered_map>
#include <algorithm>
#include <sys/stat.h>

namespace platform::system
{
    theme_t theme()
    {
        return theme_t::dark;
    }

    std::string kernel_name()
    {
        return "Linux";
    }

    version_t kernel_version()
    {
        utsname uts{};
        if (uname(&uts) == -1) {
            return {};
        }

        char* marker         = uts.release;
        const uint32_t major = std::strtoul(marker, &marker, 10);
        const uint32_t minor = std::strtoul(marker + 1, &marker, 10);
        const uint32_t patch = std::strtoul(marker + 1, &marker, 10);
        const uint32_t build = std::strtoul(marker + 1, nullptr, 10);

        return { major, minor, patch, build };
    }

    kernel_info_t kernel_info()
    {
        return {kernel_name(), kernel_version() };
    }

    static std::unordered_map<std::string, std::string> parse_kv(std::ifstream& fstream)
    {
        std::unordered_map<std::string, std::string> kvs{};

        for(std::string line; std::getline(fstream, line);) {
            auto pos = line.find('=');
            if (pos != std::string::npos) {
                auto value = line.substr(pos + 1);
                if (std::count(value.begin(), value.end(), '"') == 2) {
                    if (value[0] == value.back() && value.back() == '"') {
                        value = value.substr(1, value.size() - 2);
                    }
                }
                kvs.emplace(std::string(line.c_str(), pos), value);
            }
        }

        return kvs;
    }

    static bool file_exists(const char * name)
    {
        struct stat buffer{};
        return stat (name, &buffer) == 0;
    }

    // https://gist.github.com/natefoo/814c5bf936922dad97ff
    version_t os_version()
    {
        auto parse_version = [](std::string& str) {
            version_t version{};

            char* marker = &str[0];
            version.major = std::strtoul(marker, &marker, 10);
            version.minor = std::strtoul(marker + 1, &marker, 10);
            version.patch = std::strtoul(marker + 1, &marker, 10);
            version.build = std::strtoul(marker + 1, nullptr, 10);
            return version;
        };

        version_t ver{};
        if (file_exists("/etc/os-release")) {
            std::ifstream release("/etc/os-release");
            if(release && release.is_open()) {
                auto kvs = parse_kv(release);

                if (auto version_id = kvs.find("VERSION_ID"); version_id != kvs.end()) {
                    ver = parse_version(version_id->second);
                }
                if (auto codename = kvs.find("VERSION_CODENAME"); codename != kvs.end()) {
                    ver.codename = codename->second;
                }
            }
        }
        else if(file_exists("/etc/lsb-release")) {
            std::ifstream release("/etc/os-release");
            if(release && release.is_open()) {
                auto kvs = parse_kv(release);

                if (auto version_id = kvs.find("DISTRIB_RELEASE"); version_id != kvs.end()) {
                    ver = parse_version(version_id->second);
                }

                if (auto codename = kvs.find("DISTRIB_CODENAME"); codename != kvs.end()) {
                    ver.codename = codename->second;
                }
            }
        }

        return ver;
    }

    std::string os_name()
    {
        if (file_exists("/etc/os-release")) {
            std::ifstream release("/etc/os-release");
            if(release && release.is_open()) {
                auto kvs = parse_kv(release);
                if (auto pretty_name = kvs.find("PRETTY_NAME"); pretty_name != kvs.end()) {
                    return pretty_name->second;
                }
                else if(auto name = kvs.find("NAME"); name != kvs.end()) {
                    return  name->second;
                }
            }
        }
        else if(file_exists("/etc/lsb-release")) {
            std::ifstream release("/etc/lsb-release");
            if(release && release.is_open()) {
                auto kvs = parse_kv(release);

                if (auto desc = kvs.find("DISTRIB_DESCRIPTION"); desc != kvs.end()) {
                    return desc->second;
                }
                else if (auto name = kvs.find("DISTRIB_ID"); name != kvs.end()) {
                    return name->second;
                }
            }
        }

        return "Linux";
    }

    os_info_t os_info()
    {
        return {
            os_name(),
            theme(),
            os_version()
        };
    }
}

#endif // __linux__