#ifndef CAPTURER_LINUX_PULSE_H
#define CAPTURER_LINUX_PULSE_H

#ifdef __linux__

#include "libcap/devices.h"
extern "C" {
#include <pulse/pulseaudio.h>
}

#include <optional>
#include <string>
#include <vector>

struct PulseServerInfo
{
    std::string version;
    std::string name;
    std::string default_sink_name;
    std::string default_source_name;
};

namespace pulse
{
    void init();
    void unref();

    void loop_lock();
    void loop_unlock();

    bool context_is_ready();

    void signal(int);

    std::vector<av::device_t> source_list();
    std::vector<av::device_t> sink_list();

    std::optional<av::device_t> default_source();
    std::optional<av::device_t> default_sink();

    int server_info(PulseServerInfo& info);
} // namespace pulse

#endif // !__linux__

#endif //! CAPTURER_LINUX_PULSE_H