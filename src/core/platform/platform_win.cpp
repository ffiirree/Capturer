#ifdef _WIN32

#include "platform.h"
#include <Windows.h>
#include <processthreadsapi.h>
#include "logging.h"
#include "defer.h"

namespace platform 
{
    namespace windows
    {
        // https://learn.microsoft.com/en-us/windows/win32/sysinfo/registry-value-types
        // REG_DWORD        : A 32-bit number.
        // REG_QWORD        : A 64-bit number.
        std::optional<DWORD> reg_read_dword(HKEY key, const std::string& subkey, const std::string& valuename)
        {
            DWORD value = 0;
            DWORD size = sizeof(uint32_t);
            if (RegGetValue(key, util::to_utf16(subkey).c_str(), util::to_utf16(valuename).c_str(), 
                RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS) {
                return value;
            }

            return std::nullopt;
        }

        // REG_SZ	        : A null - terminated string. 
        //                    It's either a Unicode or an ANSI string, 
        //                    depending on whether you use the Unicode or ANSI functions.
        std::optional<std::string> reg_read_string(HKEY key, const std::string& subkey, const std::string& valuename)
        {
            DWORD size = 0;
            if (RegGetValue(key, util::to_utf16(subkey).c_str(), util::to_utf16(valuename).c_str(),
                RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS) {
                return std::nullopt;
            }

            std::string value(size, {});
            if (RegGetValue(key, util::to_utf16(subkey).c_str(), util::to_utf16(valuename).c_str(),
                RRF_RT_REG_SZ, nullptr, reinterpret_cast<LPBYTE>(value.data()), &size) != ERROR_SUCCESS) {
                return std::nullopt;
            }

            return platform::util::to_utf8(value);
        }

        int RegistryMonitor::monitor(HKEY key, const std::string& subkey, const std::function<void(HKEY)>& callback)
        {
            if (::RegOpenKeyEx(key, platform::util::to_utf16(subkey).c_str(), 0, KEY_NOTIFY, &key_) != ERROR_SUCCESS) {
                LOG(ERROR) << "failed to open the registry key : " << subkey;
                return -1;
            }

            if ((STOP_EVENT = ::CreateEvent(nullptr, TRUE, FALSE, L"Registry Stop Event")) == nullptr) {
                LOG(ERROR) << "failed to create event for the registry key : " << subkey;
                return -1;
            }

            if ((NOTIFY_EVENT = ::CreateEvent(nullptr, FALSE, FALSE, L"Registry Notify Evnent")) == nullptr) {
                LOG(ERROR) << "failed to create event for the registry key : " << subkey;
                return -1;
            }
           
            running_ = true;
            thread_ = std::thread([=]() {
                platform::util::thread_set_name("monit-registry");

                LOG(INFO) << "start monitoring key : '" << subkey << "'";

                const HANDLE events[] = { STOP_EVENT, NOTIFY_EVENT };

                while (running_) {
                    if (::RegNotifyChangeKeyValue(
                        key_, TRUE, REG_LEGAL_CHANGE_FILTER, NOTIFY_EVENT, TRUE) != ERROR_SUCCESS) {
                        LOG(ERROR) << "failed to monitor the registry key : " << subkey;
                        ::SetEvent(STOP_EVENT);
                    }

                    switch (::WaitForMultipleObjects(2, events, false, INFINITE))
                    {
                    case WAIT_OBJECT_0 + 0: // STOP_EVENT
                        running_ = false;
                        break;

                    case WAIT_OBJECT_0 + 1: // NOTIFY_EVENT
                        callback(key_);
                        break;

                    default: break;
                    }
                }

                LOG(INFO) << "stop monitoring : '" << subkey << "'";
            });

            return 0;
        }

        void RegistryMonitor::stop()
        {
            running_ = false;
            ::SetEvent(STOP_EVENT);

            if (thread_.joinable())
                thread_.join();

            ::CloseHandle(NOTIFY_EVENT);
            ::CloseHandle(STOP_EVENT);
            ::RegCloseKey(key_);
        }

        std::shared_ptr<RegistryMonitor> monitor_regkey(HKEY key, const std::string& subkey, std::function<void(HKEY)> cb)
        {
            auto monitor = std::make_shared<RegistryMonitor>();
            monitor->monitor(key, subkey, cb);
            return monitor;
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

            return std::move(wstr);
        }


        int thread_set_name(const std::string& name)
        {
            // >= Windows 10, version 1607
            if (FAILED(::SetThreadDescription(::GetCurrentThread(), to_utf16(name).c_str()))) {
                return -1;
            }
            return 0;
        }

        std::string thread_get_name()
        {
            // >= Windows 10, version 1607
            WCHAR *buffer = nullptr;
            if (SUCCEEDED(::GetThreadDescription(::GetCurrentThread(), &buffer))) {
                defer(::LocalFree(buffer));
                return to_utf8(buffer);
            }
            return {};
        }
    } // namespace util
}

#endif // _WIN32