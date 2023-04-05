#ifdef _WIN32

#include "platform.h"
#include <windows.h>

namespace platform 
{
    namespace windows
    {
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
    } // namespace windows

    namespace util
    {
        // the null-terminating-character is guaranteed by std::string
        std::string to_utf8(const wchar_t* wptr, size_t wlen)
        {
            if (!wptr) return {};

            if (wlen == 0)
                wlen = std::char_traits<wchar_t>::length(wptr);

            // non-null character
            size_t mlen = WideCharToMultiByte(CP_UTF8, 0, wptr, static_cast<int>(wlen), nullptr, 0, nullptr, nullptr);

            std::string mstr(mlen, {});
            WideCharToMultiByte(CP_UTF8, 0, wptr, static_cast<int>(wlen), mstr.data(), static_cast<int>(mstr.size()), nullptr, nullptr);

            return std::move(mstr);
        }

        std::wstring to_utf16(const char* mstr, size_t mlen)
        {
            if (!mstr) return {};

            if (mlen == 0)
                mlen = std::char_traits<char>::length(mstr);

            // non-null character
            size_t wlen = MultiByteToWideChar(CP_UTF8, 0, mstr, static_cast<int>(mlen), nullptr, 0);

            std::wstring wstr(wlen, {});
            MultiByteToWideChar(CP_UTF8, 0, mstr, static_cast<int>(mlen), wstr.data(), static_cast<int>(wstr.size()));

            return wstr;
        }
    } // namespace util
}

#endif // _WIN32