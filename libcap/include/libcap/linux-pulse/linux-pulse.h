#ifndef CAPTURER_LINUX_PULSE_H
#define CAPTURER_LINUX_PULSE_H

#ifdef __linux__

#include "libcap/devices.h"
#include "libcap/media.h"

#include <optional>
#include <string>
#include <vector>

extern "C" {
#include <pulse/pulseaudio.h>
}

struct PulseServerInfo
{
    std::string version;
    std::string name;
    std::string default_sink_name;
    std::string default_source_name;
};

// wrapper
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

    AVSampleFormat to_av_sample_format(pa_sample_format_t pa_fmt);

    uint64_t to_av_channel_layout(uint8_t channels);
} // namespace pulse

namespace pulse
{
    // server
    int server_info(PulseServerInfo& info);

    // sink & source
    std::vector<av::device_t> source_list();
    std::vector<av::device_t> sink_list();

    std::optional<av::device_t> default_source();
    std::optional<av::device_t> default_sink();

    pa_sample_spec source_format(const std::string&);

    // sink & source | input & output
    int sink_input_info(pa_stream *stream);
} // namespace pulse

namespace pulse::stream
{
    pa_stream *create(const std::string& name, const pa_sample_spec *ss, const pa_channel_map *map);

    pa_usec_t latency(pa_stream *stream);

    int cork(pa_stream *stream, bool cork, pa_stream_success_cb_t cb, void *userdata);

    int set_sink_volume(pa_stream *stream, const pa_cvolume *volume);

    float get_sink_volume(pa_stream *stream);

    int set_sink_mute(pa_stream *stream, bool muted);

    bool get_sink_muted(pa_stream *stream);

    int flush(pa_stream *stream, pa_stream_success_cb_t cb, void *userdata);

    int update_timing_info(pa_stream *stream, pa_stream_success_cb_t cb, void *userdata);
} // namespace pulse::stream

#endif // !__linux__

#endif //! CAPTURER_LINUX_PULSE_H