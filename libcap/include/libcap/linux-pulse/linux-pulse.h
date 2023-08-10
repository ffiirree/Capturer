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

    void wait();

    void signal(int);

    void accept();

    bool wait_operation(pa_operation *);

    // sink & source
    std::vector<av::device_t> source_list();
    std::vector<av::device_t> sink_list();

    std::optional<av::device_t> default_source();
    std::optional<av::device_t> default_sink();

    // server
    int server_info(PulseServerInfo& info);

    // sink & source | input & output
    int sink_input_info(pa_stream *stream);

    // stream
    pa_stream *stream_new(const std::string& name, const pa_sample_spec *ss, const pa_channel_map *map);

    pa_usec_t stream_latency(pa_stream *stream);

    int stream_cork(pa_stream *stream, bool cork, pa_stream_success_cb_t cb, void *userdata);

    int stream_set_sink_volume(pa_stream *stream, const pa_cvolume *volume);

    float stream_get_sink_volume(pa_stream *stream);

    int stream_set_sink_mute(pa_stream *stream, bool muted);

    bool stream_get_sink_muted(pa_stream *stream);

    int stream_flush(pa_stream *stream, pa_stream_success_cb_t cb, void *userdata);

    int stream_update_timing_info(pa_stream *stream, pa_stream_success_cb_t cb, void *userdata);
} // namespace pulse

#endif // !__linux__

#endif //! CAPTURER_LINUX_PULSE_H