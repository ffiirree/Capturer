#ifdef __linux__

#include "platform.h"

namespace platform
{
    namespace linux
    {
        std::optional<std::string> exec(const char* cmd)
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
    } // namespace linux

    namespace util
    {
        // TODO
        std::string to_utf8(const wchar_t* wptr, size_t wlen)
        {
            if (!wptr) return {};

            if (wlen == 0)
                wlen = std::char_traits<wchar_t>::length(wptr);

            return {};
        }

        std::wstring to_utf16(const char*, size_t)
        {
            return {};
        }
    } // namespace util
}

#endif // __linux__