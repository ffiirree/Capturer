#ifndef CAPTURER_LINUX_PULSE
#define CAPTURER_LINUX_PULSE

#ifdef __linux__

extern "C" {
    #include <pulse/pulseaudio.h>
}

struct PulseInfo
{
    PulseInfo(const char* name, const char * desc, int32_t fmt, uint32_t rate, uint8_t channels, int32_t state, bool is_monitor)
    {
        name_ = name;
        description_ = desc;
        format_ = fmt;
        rate_ = rate;
        channels_ = channels;
        state_ = state;
        is_monitor_ = is_monitor;
    }

    std::string name_;
    std::string description_;
    int32_t format_;
    uint32_t rate_;
    uint8_t channels_;
    int32_t state_;
    bool is_monitor_{false};
};

void pulse_init();
void pulse_unref();

void pulse_loop_lock();
void pulse_loop_unlock();

bool pulse_context_is_ready();

void pulse_signal(int);

std::vector<PulseInfo> pulse_get_source_info_list();
std::vector<PulseInfo> pulse_get_sink_info_list();

void pulse_get_source_info();
void pulse_get_sink_info();

#endif // !__linux__

#endif // !CAPTURER_LINUX_PULSE