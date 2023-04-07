#ifdef __linux__

#include "platform.h"
#include <chrono>
#include "logging.h"


using namespace std::chrono_literals;

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

        int GSettingsMonitor::monitor(const std::string& key, const std::string& subkey, std::function<void(const std::string&)> callback)
        {
            const std::string cmd = "gsettings monitor " + key + " " + subkey;

            running_ = true;
            thread_ = std::thread([=](){
                platform::util::thread_set_name("monit-gsettings");

                while (running_) {
                    LOG(INFO) << "open '" << cmd << "'";

                    FILE* pipe = popen(cmd.c_str(), "r");

                    if (!pipe) {
                        LOG(ERROR) << "failed to open : " << cmd;
                        std::this_thread::sleep_for(100ms);
                        continue;
                    }
                    
                    char buffer[256]{};
                    while (running_ && (fgets(buffer, sizeof(buffer), pipe) != nullptr)) {
                        callback(buffer);
                    }

                    pclose(pipe);       // TODO: stuck if the cmd process does not exit

                    // exit unexpectedly
                    if (running_) {
                        LOG(WARNING) << "exit unexpectedly, try monitor the '" << cmd << "' again after 250ms";
                        std::this_thread::sleep_for(250ms);
                    }
                }
            });

            return 0;
        }

        void GSettingsMonitor::stop()
        {
            running_ = false;

            if (thread_.joinable())
                thread_.join();
        }

        std::shared_ptr<GSettingsMonitor> monitor_gsettings(const std::string& key, const std::string& subkey, std::function<void(const std::string&)> cb)
        {
            auto monitor = std::make_shared<GSettingsMonitor>();
            monitor->monitor(key, subkey, cb);
            return monitor;
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

        int thread_set_name(const std::string& name)
        {
            return pthread_setname_np(pthread_self(), name.substr(0, 15).c_str());
        }

        std::string thread_get_name()
        {
            char buffer[128];
            pthread_getname_np(pthread_self(), buffer, 128);
            return buffer;
        }
    } // namespace util
}

#endif // __linux__